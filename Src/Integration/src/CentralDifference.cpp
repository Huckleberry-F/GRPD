// ============================================================================
// CentralDifference.cpp - 中心差分时间积分器实现
// ============================================================================

#include "CentralDifference.h"
#include "BCManager.h"
#include "FieldManager.h"
#include "Logger.h"
#include "PDKernel.h"
#include "ParticleManager.h"
#include "TypedField.h"
#include "TimeIntegratorRegistry.h"
#include <chrono>
#include <omp.h>

namespace Src::Integration {

void CentralDifference::run(PDCommon::Core::PDContext &ctx,
                            std::vector<std::unique_ptr<PDKernel>> &kernels,
                            std::function<void(int, double)> outputCallback) {

  const double dt = dt_;
  const int totalSteps = static_cast<int>(totalTime_ / dt_);
  const int outputInterval = outputInterval_;

  auto &fieldManager = ctx.getFieldManager();
  auto &bcManager = ctx.getBCManager();
  const size_t numParticles = ctx.getParticleManager().getTotalParticles();

  // 从所有内核支持汇总的分量场中搜索二阶 ODE
  std::vector<PDKernel::IntegrationTarget> targets;
  for (auto &kernel : kernels) {
    auto t = kernel->getIntegrationTargets();
    targets.insert(targets.end(), t.begin(), t.end());
  }

  // 定义泛型二阶场指针结构体
  struct SecondOrderTarget {
    std::string uName, vName, aName;
    double *uPtr;
    double *vPtr;
    double *aPtr;
    size_t totalComponents;
  };
  std::vector<SecondOrderTarget> soTargets;

  // 泛型拓扑检测：自动寻找满足二阶常微分方程依赖链的 target 对
  // target_a: (Primary: V, Rate: A)  --> 负责更新速度
  // target_b: (Primary: U, Rate: V)  --> 负责更新位移
  // 条件：target_b 的 rateField == target_a 的 primaryField
  for (const auto &tb : targets) {
    for (const auto &ta : targets) {
      if (tb.rateField == ta.primaryField && tb.dimension == ta.dimension) {
        auto *uField = fieldManager.getFieldAs<double>(tb.primaryField);
        auto *vField = fieldManager.getFieldAs<double>(ta.primaryField);
        auto *aField = fieldManager.getFieldAs<double>(ta.rateField);

        if (uField && vField && aField) {
            soTargets.push_back({
                tb.primaryField, ta.primaryField, ta.rateField,
                uField->dataPtr(), vField->dataPtr(), aField->dataPtr(),
                numParticles * tb.dimension
            });
            LOG_INFO("[CentralDifference] Identified generic 2nd-order ODE couple: " 
                     + tb.primaryField + " (u) <- " + ta.primaryField + " (v) <- " + ta.rateField + " (a)");
        }
      }
    }
  }

  if (soTargets.empty()) {
    LOG_ERROR("[CentralDifference] No valid 2nd-order target pairs found (e.g. Rate of Target_B == Primary of Target_A). "
              "Central Difference strictly requires coupled 2nd-order ODEs (like Mechanics).");
    exit(EXIT_FAILURE);
  }

  LOG_INFO("[CentralDifference] Time loop: dt = " + std::to_string(dt) +
           ", totalSteps = " + std::to_string(totalSteps));

  // 预计算（形状张量等，遍历所有内核各调用一次）
  for (auto &kernel : kernels) {
    kernel->preCompute(ctx);
  }

  // 初始化步：清零所有加速度场(最高阶率场)，计算 a_0
  for (auto &so : soTargets) {
    auto *aField = fieldManager.getFieldAs<double>(so.aName);
    if (aField) aField->clearToZero();
  }
  bcManager.applySources();
  bcManager.applyConstraints();
  for (auto &kernel : kernels) {
    kernel->computeForceState(ctx);
  }

  auto tStart = std::chrono::high_resolution_clock::now();
  double pureComputeTime = 0.0;

  for (int step = 0; step <= totalSteps; ++step) {

    // (A) 输出结果 + 打印进度
    if (step % outputInterval == 0) {
      auto tNow = std::chrono::high_resolution_clock::now();
      double totalElapsed = std::chrono::duration<double>(tNow - tStart).count();
      double pureSpeed = (step > 0 && pureComputeTime > 0.0) ? (step / pureComputeTime) : 0.0;

      LOG_INFO("--- Step " + std::to_string(step) + " / " + std::to_string(totalSteps) +
               "  |  Pure Compute: " + std::to_string(pureComputeTime) + "s" +
               "  |  Total: " + std::to_string(totalElapsed) + "s" +
               "  |  Pure Speed: " + std::to_string(pureSpeed) + " steps/s");

      LOG_INFO("Starting data export process...");
      if (outputCallback)
        outputCallback(step, step * dt);
    }
    if (step == totalSteps)
      break;

    auto tComputeStart = std::chrono::high_resolution_clock::now();

    // -------------------------------------------------------------
    // Generic Velocity Verlet (Leapfrog) - For any 2nd-order system
    // -------------------------------------------------------------

    // 1. v_{n+1/2} = v_n + 0.5 * a_n * dt
    //    u_{n+1}   = u_n + v_{n+1/2} * dt
    for (auto &so : soTargets) {
      #pragma omp parallel for schedule(static)
      for (int i = 0; i < static_cast<int>(so.totalComponents); ++i) {
        so.vPtr[i] += 0.5 * so.aPtr[i] * dt;
        so.uPtr[i] += so.vPtr[i] * dt;
      }
    }

    // 2. Clear rate fields (只清零最高阶的 Acceleration)
    for (auto &so : soTargets) {
      auto *aField = fieldManager.getFieldAs<double>(so.aName);
      if (aField) aField->clearToZero();
    }

    // 3. Evaluate forces (Updates a_{n+1})
    bcManager.applySources();
    bcManager.applyConstraints();
    for (auto &kernel : kernels) {
      kernel->computeForceState(ctx);
    }

    // 4. v_{n+1} = v_{n+1/2} + 0.5 * a_{n+1} * dt
    for (auto &so : soTargets) {
      #pragma omp parallel for schedule(static)
      for (int i = 0; i < static_cast<int>(so.totalComponents); ++i) {
        so.vPtr[i] += 0.5 * so.aPtr[i] * dt;
      }
    }

    // 约束覆盖
    bcManager.applyConstraints();

    // 后处理钩子：遍历所有内核执行 postCompute（状态变量演化等）
    for (auto &kernel : kernels) {
      kernel->postCompute(ctx);
    }

    auto tComputeEnd = std::chrono::high_resolution_clock::now();
    pureComputeTime += std::chrono::duration<double>(tComputeEnd - tComputeStart).count();
  }

  auto tEnd = std::chrono::high_resolution_clock::now();
  double totalElapsed = std::chrono::duration<double>(tEnd - tStart).count();
  LOG_INFO("[CentralDifference] Finished. Total: " + std::to_string(totalElapsed) +
           "s  |  Pure Compute avg: " + std::to_string(pureComputeTime / totalSteps) + " s/step");
}

} // namespace Src::Integration

REGISTER_TIME_INTEGRATOR(CentralDifference, Src::Integration::CentralDifference)
