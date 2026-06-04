// ============================================================================
// CentralDifference.cpp - 中心差分时间积分器实现
// ============================================================================

#include "CentralDifference.h"
#include "Logger.h"
#include "PDKernel.h"
#include "StringUtils.h"
#include "TimeIntegratorRegistry.h"
#include "Timer.h"
#include <omp.h>


namespace Src::Integration {

using PDCommon::BC::BC;

void CentralDifference::configure(const YAML::Node &solverNode) {
  TimeIntegrator::configure(solverNode);
  // 可在此处追加解析 CentralDifference 特有的 YAML 参数
}

void CentralDifference::run(PDCommon::Core::PDContext &ctx,
                            std::vector<std::unique_ptr<PDKernel>> &kernels,
                            std::function<void(int, double)> outputCallback) {

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

  double limitDt = computeCFLTimestep(ctx);
  if (autoCalcDt_) {
    dt_ = limitDt;
    LOG_INFO("[CentralDifference] Auto CFL enabled. Setting dt = " +
             PDCommon::Utils::StringUtils::toScientific(dt_));
  } else {
    if (dt_ > limitDt) {
      LOG_WARNING("[CentralDifference] Global TimeStep_dt (" +
                  PDCommon::Utils::StringUtils::toScientific(dt_) +
                  ") EXCEEDS safe CFL limit (" +
                  PDCommon::Utils::StringUtils::toScientific(limitDt) +
                  ")! Auto-clamping global dt to the safe limit.");
      dt_ = limitDt;
    }
  }

  // Verify inner load steps dt as well
  for (auto &config : loadStepConfigs_) {
    if (config.userDt > limitDt) {
      LOG_WARNING("[CentralDifference] LoadStep " +
                  std::to_string(config.stepId) + " TimeStep_dt (" +
                  PDCommon::Utils::StringUtils::toScientific(config.userDt) +
                  ") EXCEEDS safe CFL limit! Auto-clamping to safe limit.");
      config.userDt = limitDt;
    }
  }

  LOG_INFO("[CentralDifference] Starting Explicit Loop with " +
           std::to_string(loadStepConfigs_.size()) +
           " LoadStep(s). Default dt = " +
           PDCommon::Utils::StringUtils::toScientific(dt_));

  int initialStepId = loadStepConfigs_.empty() ? 0 : loadStepConfigs_[0].stepId;
  int initKbc =
      loadStepConfigs_.empty()
          ? kbc_
          : (loadStepConfigs_[0].kbc >= 0 ? loadStepConfigs_[0].kbc : kbc_);
  double initLF = (initKbc == 1) ? 1.0 : 0.0;
  BC::applyConstraints(ctx.getBCManager(), initLF, initialStepId);
  ctx.setCurrentDt(dt_); // 初始化时同步 dt
  evaluateForces(ctx, kernels, accFieldNames_, initialStepId, initLF);

  PDCommon::Utils::Timer timer;
  timer.start();

  int globalStepCounter = 0;
  double currentTime = 0.0;
  const int outputInterval = outputInterval_;

  for (size_t lstepIdx = 0; lstepIdx < loadStepConfigs_.size(); ++lstepIdx) {
    const auto &config = loadStepConfigs_[lstepIdx];
    double targetTime = config.targetTime;
    double currentDt = (config.userDt > 0.0) ? config.userDt : dt_;
    int currentKbc = (config.kbc >= 0) ? config.kbc : kbc_;

    LOG_INFO("=== Load Step " + std::to_string(config.stepId) + " / " +
             std::to_string(loadStepConfigs_.size()) + " | TargetTime: " +
             PDCommon::Utils::StringUtils::toScientific(targetTime) +
             " | dt: " + PDCommon::Utils::StringUtils::toScientific(currentDt) +
             " | KBC: " + std::to_string(currentKbc) + " ===");

    while (currentTime < targetTime - 1e-5 * currentDt) {
      if (globalStepCounter % outputInterval == 0) {
        LOG_INFO(
            "--- Step " + std::to_string(globalStepCounter) + " | Time: " +
            PDCommon::Utils::StringUtils::toScientific(currentTime) +
            "  |  Pure Compute: " +
            PDCommon::Utils::StringUtils::toScientific(
                timer.pureComputeTime()) +
            "s" + "  |  Total: " +
            PDCommon::Utils::StringUtils::toScientific(timer.totalElapsed()) +
            "s" + "  |  Speed: " +
            std::to_string(static_cast<int>(timer.pureSpeed())) + " steps/s");

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

      double prevTargetTime =
          (lstepIdx == 0) ? 0.0 : loadStepConfigs_[lstepIdx - 1].targetTime;
      double stepLoadFactor = 1.0;
      if (targetTime > prevTargetTime) {
        stepLoadFactor = (currentTime + currentDt - prevTargetTime) /
                         (targetTime - prevTargetTime);
      }
      int currentKbc = (config.kbc >= 0) ? config.kbc : kbc_;
      double activeLF = (currentKbc == 1) ? 1.0 : stepLoadFactor;

      updateKinematicsStep1(currentDt);
      BC::applyConstraints(ctx.getBCManager(), activeLF, config.stepId);

      // 中心差分每一步都是真实的物理时间推进，拓扑必须实时更新
      ctx.setIncrementStart(true);
      ctx.setCurrentDt(currentDt); // 同步真实 dt 给接触模块
      evaluateForces(ctx, kernels, accFieldNames_, config.stepId, activeLF);

      updateKinematicsStep2(currentDt);
      BC::applyConstraints(ctx.getBCManager(), activeLF, config.stepId);

      for (auto &kernel : kernels) {
        kernel->postCompute(ctx);
      }

      // [状态递进]：非常关键！必须更新本构材料历史变量（如塑性应变 EqPS）
      // 1. 中央场管理器统一执行所有具有 O(1) Swap
      // 互换资格的物理场对，完美规避多材料双重互换 Bug
      ctx.getFieldManager().executeAllRegisteredSwaps();

      // 2. 依次通知所有材料实例，令其同步全局已被互换更新的最新内存指针
      for (auto &[matName, matPtr] : ctx.getMaterialManager().getMaterials()) {
        if (matPtr)
          matPtr->commitState();
      }
      // -------------------------------------------------------------

      timer.tock();

      currentTime += currentDt;
      globalStepCounter++;
    }

    // 该载荷步物理时间推进完毕，通知所有 BC 将当前极值固化为下一步的起点
    BC::commitEndStep(ctx.getBCManager());
  }

  // 最终强制输出一次作为结束
  LOG_INFO(
      "--- Step " + std::to_string(globalStepCounter) +
      " | Time: " + PDCommon::Utils::StringUtils::toScientific(currentTime) +
      "  |  Pure Compute: " +
      PDCommon::Utils::StringUtils::toScientific(timer.pureComputeTime()) +
      "s" + "  |  Total: " +
      PDCommon::Utils::StringUtils::toScientific(timer.totalElapsed()) + "s" +
      "  |  Speed: " + std::to_string(static_cast<int>(timer.pureSpeed())) +
      " steps/s");
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
