// ============================================================================
// ExplicitEuler.cpp - 显式欧拉时间积分器实现（多核协同版本）
//
// 从原 Solve.cpp 中抽取时间循环逻辑，通过 PDKernel 接口通用化。
// 支持同时驱动多个物理场内核（如 Thermal + Mechanical），
// 在每个时间步统一遍历触发 preCompute / computeForceState / postCompute。
// ============================================================================

#include "ExplicitEuler.h"
#include "BCManager.h"
#include "FieldManager.h"
#include "Logger.h"
#include "PDKernel.h"
#include "ParticleManager.h"
#include "TypedField.h"
#include <chrono>
#include <omp.h>

namespace Src::Integration {

void ExplicitEuler::run(PDCommon::Core::PDContext &ctx,
                        std::vector<std::unique_ptr<PDKernel>> &kernels,
                        std::function<void(int, double)> outputCallback) {

  const double dt = dt_;
  const int totalSteps = static_cast<int>(totalTime_ / dt_);
  const int outputInterval = outputInterval_;

  auto &fieldManager = ctx.getFieldManager();
  auto &bcManager = ctx.getBCManager();
  const size_t numParticles = ctx.getParticleManager().getTotalParticles();

  // =================================================================
  // 汇总所有内核声明的积分目标场（多核场合并）
  // =================================================================
  struct FieldPtrs {
    double *primaryPtr;
    double *ratePtr;
    size_t totalComponents; // numParticles * dimension
  };
  std::vector<FieldPtrs> fieldPtrs;
  std::vector<std::string> rateFieldNames; // 率场名称列表（供 evaluateForces 清零）

  for (auto &kernel : kernels) {
    auto targets = kernel->getIntegrationTargets();
    for (const auto &target : targets) {
      auto *primaryField =
          fieldManager.getFieldAs<double>(target.primaryField);
      auto *rateField = fieldManager.getFieldAs<double>(target.rateField);

      if (!primaryField || !rateField) {
        LOG_ERROR("[ExplicitEuler] Field '" + target.primaryField + "' or '" +
                  target.rateField + "' not found!");
        return;
      }

      fieldPtrs.push_back({primaryField->dataPtr(), rateField->dataPtr(),
                           numParticles * target.dimension});
      rateFieldNames.push_back(target.rateField);
    }
  }

  LOG_INFO("[ExplicitEuler] Time loop: dt = " + std::to_string(dt) +
           ", totalSteps = " + std::to_string(totalSteps) +
           ", kernels = " + std::to_string(kernels.size()));

  // =================================================================
  // 预计算（形状张量等，遍历所有内核各调用一次）
  // =================================================================
  for (auto &kernel : kernels) {
    kernel->preCompute(ctx);
  }

  // 初始化：设定 Dirichlet 约束初值
  bcManager.applyConstraints();

  // =================================================================
  // 时间步循环
  // =================================================================
  auto tStart = std::chrono::high_resolution_clock::now();
  double pureComputeTime = 0.0;

  for (int step = 0; step <= totalSteps; ++step) {

    // (A) 输出结果 + 打印进度
    if (step % outputInterval == 0) {
      auto tNow = std::chrono::high_resolution_clock::now();
      double totalElapsed =
          std::chrono::duration<double>(tNow - tStart).count();

      double pureSpeed =
          (step > 0 && pureComputeTime > 0.0) ? (step / pureComputeTime) : 0.0;

      LOG_INFO("--- Step " + std::to_string(step) + " / " +
               std::to_string(totalSteps) +
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

    // (B,C,D) 清零率场 → 施加 Neumann 源项 → 计算内力
    evaluateForces(ctx, kernels, rateFieldNames);

    // (E) 显式时间积分：primary += rate * dt
    for (auto &fp : fieldPtrs) {
#pragma omp parallel for schedule(static)
      for (int i = 0; i < static_cast<int>(fp.totalComponents); ++i) {
        fp.primaryPtr[i] += fp.ratePtr[i] * dt;
      }
    }

    // (F) 约束覆盖（Dirichlet 条件积分后重新施加）
    bcManager.applyConstraints();

    // (G) 后处理钩子：遍历所有内核执行 postCompute（状态变量演化等）
    for (auto &kernel : kernels) {
      kernel->postCompute(ctx);
    }

    // 累加纯计算时间
    auto tComputeEnd = std::chrono::high_resolution_clock::now();
    pureComputeTime +=
        std::chrono::duration<double>(tComputeEnd - tComputeStart).count();
  }

  auto tEnd = std::chrono::high_resolution_clock::now();
  double totalElapsed = std::chrono::duration<double>(tEnd - tStart).count();
  LOG_INFO("[ExplicitEuler] Finished. Total: " + std::to_string(totalElapsed) +
           "s  |  Pure Compute avg: " +
           std::to_string(pureComputeTime / totalSteps) + " s/step");
}

} // namespace Src::Integration

#include "TimeIntegratorRegistry.h"

REGISTER_TIME_INTEGRATOR(Explicit, Src::Integration::ExplicitEuler)
