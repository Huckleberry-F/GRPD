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

  LOG_INFO("[CentralDifference] Starting Explicit Loop with " + 
           std::to_string(loadStepConfigs_.size()) + " LoadStep(s). Default dt = " + std::to_string(dt_));

  int initialStepId = loadStepConfigs_.empty() ? 0 : loadStepConfigs_[0].stepId;
  int initKbc = loadStepConfigs_.empty() ? kbc_ : (loadStepConfigs_[0].kbc >= 0 ? loadStepConfigs_[0].kbc : kbc_);
  double initLF = (initKbc == 1) ? 1.0 : 0.0;
  bcManager.applyConstraints(initLF, initialStepId);
  evaluateForces(ctx, kernels, accFieldNames_);

  PDCommon::Utils::Timer timer;
  timer.start();

  int globalStepCounter = 0;
  double currentTime = 0.0;
  const int outputInterval = outputInterval_;

  for (size_t lstepIdx = 0; lstepIdx < loadStepConfigs_.size(); ++lstepIdx) {
    const auto& config = loadStepConfigs_[lstepIdx];
    double targetTime = config.targetTime;
    double currentDt = (config.userDt > 0.0) ? config.userDt : dt_; 
    int currentKbc = (config.kbc >= 0) ? config.kbc : kbc_;

    LOG_INFO("=== Load Step " + std::to_string(config.stepId) + " / " +
             std::to_string(loadStepConfigs_.size()) + 
             " | TargetTime: " + std::to_string(targetTime) + 
             " | dt: " + std::to_string(currentDt) +
             " | KBC: " + std::to_string(currentKbc) + " ===");

    while (currentTime < targetTime - 1e-12) {
      if (globalStepCounter % outputInterval == 0) {
        LOG_INFO("--- Step " + std::to_string(globalStepCounter) + " | Time: " +
                 std::to_string(currentTime) +
                 "  |  Pure Compute: " + std::to_string(timer.pureComputeTime()) + "s" +
                 "  |  Total: " + std::to_string(timer.totalElapsed()) + "s" +
                 "  |  Speed: " + std::to_string(static_cast<int>(timer.pureSpeed())) + " steps/s");

        if (outputCallback)
          outputCallback(globalStepCounter, currentTime);
      }

      // 调整步长确保准确着陆到 targetTime
      if (currentTime + currentDt > targetTime) {
        currentDt = targetTime - currentTime;
      }

      timer.tick();

      // -------------------------------------------------------------
      // Generic Velocity Verlet (Leapfrog) Abstract Pipeline
      // -------------------------------------------------------------
      
      double prevTargetTime = (lstepIdx == 0) ? 0.0 : loadStepConfigs_[lstepIdx - 1].targetTime;
      double stepLoadFactor = 1.0;
      if (targetTime > prevTargetTime) {
         stepLoadFactor = (currentTime + currentDt - prevTargetTime) / (targetTime - prevTargetTime);
      }
      int currentKbc = (config.kbc >= 0) ? config.kbc : kbc_;
      double activeLF = (currentKbc == 1) ? 1.0 : stepLoadFactor;

      updateKinematicsStep1(currentDt);
      bcManager.applyConstraints(activeLF, config.stepId);

      evaluateForces(ctx, kernels, accFieldNames_);

      updateKinematicsStep2(currentDt);
      bcManager.applyConstraints(activeLF, config.stepId);

      for (auto &kernel : kernels) {
        kernel->postCompute(ctx);
      }
      // -------------------------------------------------------------

      timer.tock();

      currentTime += currentDt;
      globalStepCounter++;
    }
    
    // 该载荷步物理时间推进完毕，通知所有 BC 将当前极值固化为下一步的起点
    bcManager.commitEndStep();
  }

  // 最终强制输出一次作为结束
  if (outputCallback) {
    outputCallback(globalStepCounter, currentTime);
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
