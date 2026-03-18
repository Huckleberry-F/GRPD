// ============================================================================
// ExplicitEuler.cpp - 显式欧拉时间积分器实现
//
// 从原 Solve.cpp 中抽取时间循环逻辑，通过 PDKernel 接口通用化。
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

namespace Src::Solve {

void ExplicitEuler::run(PDCommon::Core::PDContext &ctx, PDKernel &kernel,
                        const SolverConfig &config,
                        std::function<void()> outputCallback) {

  const double dt = config.dt;
  const int totalSteps = static_cast<int>(config.totalTime / dt);
  const int outputInterval = config.outputInterval;

  auto &fieldManager = ctx.getFieldManager();
  auto &bcManager = ctx.getBCManager();
  const size_t numParticles = ctx.getParticleManager().getTotalParticles();

  // =================================================================
  // 获取积分目标场（由 PDKernel 声明需要更新的场）
  // =================================================================
  auto targets = kernel.getIntegrationTargets();

  // 预先获取场指针，避免每步查找
  struct FieldPtrs {
    double *primaryPtr;
    double *ratePtr;
    size_t totalComponents; // numParticles * dimension
  };
  std::vector<FieldPtrs> fieldPtrs;
  fieldPtrs.reserve(targets.size());

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
  }

  LOG_INFO("[ExplicitEuler] Time loop: dt = " + std::to_string(dt) +
           ", totalSteps = " + std::to_string(totalSteps));

  // =================================================================
  // 预计算（形状张量等，仅调用一次）
  // =================================================================
  kernel.preCompute(ctx);

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
        outputCallback();
    }
    if (step == totalSteps)
      break;

    auto tComputeStart = std::chrono::high_resolution_clock::now();

    // (B) 清零所有变化率场
    for (const auto &target : targets) {
      auto *rateField = fieldManager.getFieldAs<double>(target.rateField);
      rateField->clearToZero();
    }

    // (C) 施加边界条件源项 + 约束
    bcManager.applySources();
    bcManager.applyConstraints();

    // (D) PD 核心积分（黑盒调用 L2）
    kernel.computeForceState(ctx);

    // (E) 显式时间积分：primary += rate * dt
    for (auto &fp : fieldPtrs) {
#pragma omp parallel for schedule(static)
      for (int i = 0; i < static_cast<int>(fp.totalComponents); ++i) {
        fp.primaryPtr[i] += fp.ratePtr[i] * dt;
      }
    }

    // (F) 约束覆盖（Dirichlet 条件积分后重新施加）
    bcManager.applyConstraints();

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

} // namespace Src::Solve
