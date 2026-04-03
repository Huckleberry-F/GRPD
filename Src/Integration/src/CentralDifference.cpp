// ============================================================================
// CentralDifference.cpp - 中心差分时间积分器实现
// ============================================================================

#include "CentralDifference.h"
#include "BCManager.h"
#include "Logger.h"
#include "PDKernel.h"
#include "TimeIntegratorRegistry.h"
#include "Timer.h"
#include <omp.h>

namespace Src::Integration {

void CentralDifference::configure(const YAML::Node &solverNode) {
  TimeIntegrator::configure(solverNode);
  // 可在此处追加解析 CentralDifference 特有的 YAML 参数
}

void CentralDifference::run(PDCommon::Core::PDContext &ctx,
                            std::vector<std::unique_ptr<PDKernel>> &kernels,
                            std::function<void(int, double)> outputCallback) {

  auto &bcManager = ctx.getBCManager();

  // 1. 获取物理关联
  extractSecondOrderTargets(kernels, ctx, soTargets_, accFieldNames_);
  if (soTargets_.empty()) {
    LOG_ERROR("[CentralDifference] No valid 2nd-order target pairs found. "
              "Central Difference strictly requires coupled 2nd-order ODEs.");
    exit(EXIT_FAILURE);
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

  LOG_INFO("[CentralDifference] Time loop: dt = " + std::to_string(dt) +
           ", totalSteps = " + std::to_string(totalSteps));

  bcManager.applyConstraints();
  evaluateForces(ctx, kernels, accFieldNames_);

  PDCommon::Utils::Timer timer;
  timer.start();

  for (int step = 0; step <= totalSteps; ++step) {

    if (step % outputInterval == 0) {
      LOG_INFO("--- Step " + std::to_string(step) + " / " +
               std::to_string(totalSteps) +
               "  |  Pure Compute: " + std::to_string(timer.pureComputeTime()) + "s" +
               "  |  Total: " + std::to_string(timer.totalElapsed()) + "s" +
               "  |  Speed: " + std::to_string(static_cast<int>(timer.pureSpeed())) + " steps/s");

      LOG_INFO("Starting data export process...");
      if (outputCallback)
        outputCallback(step, step * dt);
    }
    if (step == totalSteps)
      break;

    timer.tick();

    // -------------------------------------------------------------
    // Generic Velocity Verlet (Leapfrog) Abstract Pipeline
    // -------------------------------------------------------------
    updateKinematicsStep1(dt);
    bcManager.applyConstraints();

    evaluateForces(ctx, kernels, accFieldNames_);

    updateKinematicsStep2(dt);
    bcManager.applyConstraints();

    for (auto &kernel : kernels) {
      kernel->postCompute(ctx);
    }
    // -------------------------------------------------------------

    timer.tock();
  }

  timer.logSummary("CentralDifference");
}

void CentralDifference::updateKinematicsStep1(double dt) {
  for (auto &so : soTargets_) {
#pragma omp parallel for schedule(static)
    for (int i = 0; i < static_cast<int>(so.totalComponents); ++i) {
      so.vPtr[i] += 0.5 * so.aPtr[i] * dt;
      so.uPtr[i] += so.vPtr[i] * dt;
    }
  }
}

void CentralDifference::updateKinematicsStep2(double dt) {
  for (auto &so : soTargets_) {
#pragma omp parallel for schedule(static)
    for (int i = 0; i < static_cast<int>(so.totalComponents); ++i) {
      so.vPtr[i] += 0.5 * so.aPtr[i] * dt;
    }
  }
}

} // namespace Src::Integration

REGISTER_TIME_INTEGRATOR(CentralDifference, Src::Integration::CentralDifference)
