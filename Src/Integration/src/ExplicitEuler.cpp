// ============================================================================
// ExplicitEuler.cpp - 显式欧拉时间积分器实现（多核协同版本）
// ============================================================================

#include "ExplicitEuler.h"
#include "BCManager.h"
#include "Logger.h"
#include "PDKernel.h"
#include <chrono>
#include <omp.h>

namespace Src::Integration {

void ExplicitEuler::configure(const YAML::Node &solverNode) {
  TimeIntegrator::configure(solverNode);
  // 可在此处追加解析 ExplicitEuler 特有的 YAML 参数
}

void ExplicitEuler::run(PDCommon::Core::PDContext &ctx,
                        std::vector<std::unique_ptr<PDKernel>> &kernels,
                        std::function<void(int, double)> outputCallback) {

  auto &bcManager = ctx.getBCManager();

  // 1. 获取一阶积分关联拓扑 (如 T -> T_rate)
  extractFirstOrderTargets(kernels, ctx, foTargets_, rateFieldNames_);
  if (foTargets_.empty()) {
    LOG_WARNING("[ExplicitEuler] No valid 1st-order target pairs found. "
                "Running empty loop?");
  }

  for (auto &kernel : kernels) {
    kernel->preCompute(ctx);
  }

  if (autoCalcDt_) {
    computeCFLTimestep(ctx);
  }

  const double dt = dt_;
  const int totalSteps = static_cast<int>(std::lround(totalTime_ / dt));
  const int outputInterval = outputInterval_;

  LOG_INFO("[ExplicitEuler] Time loop: dt = " + std::to_string(dt) +
           ", totalSteps = " + std::to_string(totalSteps) +
           ", kernels = " + std::to_string(kernels.size()));

  bcManager.applyConstraints();

  auto tStart = std::chrono::high_resolution_clock::now();
  double pureComputeTime = 0.0;

  for (int step = 0; step <= totalSteps; ++step) {

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

    // -------------------------------------------------------------
    // Generic Explicit Euler Pipeline
    // -------------------------------------------------------------
    evaluateForces(ctx, kernels, rateFieldNames_);

    updateKinematicsEuler(dt);

    bcManager.applyConstraints();

    for (auto &kernel : kernels) {
      kernel->postCompute(ctx);
    }
    // -------------------------------------------------------------

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

void ExplicitEuler::updateKinematicsEuler(double dt) {
  for (auto &fp : foTargets_) {
#pragma omp parallel for schedule(static)
    for (int i = 0; i < static_cast<int>(fp.totalComponents); ++i) {
      fp.primaryPtr[i] += fp.ratePtr[i] * dt;
    }
  }
}

} // namespace Src::Integration

#include "TimeIntegratorRegistry.h"
REGISTER_TIME_INTEGRATOR(Explicit, Src::Integration::ExplicitEuler)
