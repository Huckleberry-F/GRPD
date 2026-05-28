// ============================================================================
// ExplicitEuler.cpp - 显式欧拉时间积分器实现（多核协同版本）
// ============================================================================

#include "ExplicitEuler.h"
#include "BCManager.h"
#include "FieldManager.h"
#include "Logger.h"
#include "StringUtils.h"
#include "MaterialManager.h"
#include "PDKernel.h"
#include "ThermalMaterial.h"
#include "Timer.h"
#include <cmath>
#include <omp.h>


namespace Src::Integration {

using PDCommon::BC::BC;

void ExplicitEuler::configure(const YAML::Node &solverNode) {
  TimeIntegrator::configure(solverNode);
  // 可在此处追加解析 ExplicitEuler 特有的 YAML 参数
}

void ExplicitEuler::run(PDCommon::Core::PDContext &ctx,
                        std::vector<std::unique_ptr<PDKernel>> &kernels,
                        std::function<void(int, double)> outputCallback) {

  // 1. 获取一阶积分关联拓扑 (如 T -> T_rate)
  extractFirstOrderTargets(kernels, ctx, foTargets_, rateFieldNames_);
  if (foTargets_.empty()) {
    LOG_WARNING("[ExplicitEuler] No valid 1st-order target pairs found. "
                "Running empty loop?");
  }

  for (auto &kernel : kernels) {
    kernel->preCompute(ctx);
  }

  double limitDt = computeCFLTimestep(ctx);
  if (autoCalcDt_) {
    dt_ = limitDt;
    LOG_INFO("[ExplicitEuler] Auto CFL enabled. Setting dt = " + PDCommon::Utils::StringUtils::toScientific(dt_));
  } else {
    if (dt_ > limitDt) {
      LOG_WARNING("[ExplicitEuler] Global TimeStep_dt (" + PDCommon::Utils::StringUtils::toScientific(dt_) + 
                  ") EXCEEDS safe CFL limit (" + PDCommon::Utils::StringUtils::toScientific(limitDt) + 
                  ")! Auto-clamping global dt to the safe limit.");
      dt_ = limitDt;
    }
  }

  // Verify inner load steps dt as well
  for (auto &config : loadStepConfigs_) {
    if (config.userDt > limitDt) {
      LOG_WARNING("[ExplicitEuler] LoadStep " + std::to_string(config.stepId) + 
                  " TimeStep_dt (" + PDCommon::Utils::StringUtils::toScientific(config.userDt) + 
                  ") EXCEEDS safe CFL limit! Auto-clamping to safe limit.");
      config.userDt = limitDt;
    }
  }

  LOG_INFO("[ExplicitEuler] Starting Explicit Loop with " +
           std::to_string(loadStepConfigs_.size()) +
           " LoadStep(s). Default dt = " + PDCommon::Utils::StringUtils::toScientific(dt_));

  int initialStepId = loadStepConfigs_.empty() ? 0 : loadStepConfigs_[0].stepId;
  int initKbc =
      loadStepConfigs_.empty()
          ? kbc_
          : (loadStepConfigs_[0].kbc >= 0 ? loadStepConfigs_[0].kbc : kbc_);
  double initLF = (initKbc == 1) ? 1.0 : 0.0;
  BC::applyConstraints(ctx.getBCManager(), initLF, initialStepId);

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
             std::to_string(loadStepConfigs_.size()) +
             " | TargetTime: " + PDCommon::Utils::StringUtils::toScientific(targetTime) +
             " | dt: " + PDCommon::Utils::StringUtils::toScientific(currentDt) +
             " | KBC: " + std::to_string(currentKbc) + " ===");

    while (currentTime < targetTime - 1e-12) {
      if (globalStepCounter % outputInterval == 0) {
        LOG_INFO(
            "--- Step " + std::to_string(globalStepCounter) +
            " | Time: " + PDCommon::Utils::StringUtils::toScientific(currentTime) +
            "  |  Pure Compute: " + PDCommon::Utils::StringUtils::toScientific(timer.pureComputeTime()) +
            "s" + "  |  Total: " + PDCommon::Utils::StringUtils::toScientific(timer.totalElapsed()) + "s" +
            "  |  Speed: " +
            std::to_string(static_cast<int>(timer.pureSpeed())) + " steps/s");

        if (outputCallback)
          outputCallback(globalStepCounter, currentTime);
      }

      if (currentTime + currentDt > targetTime) {
        currentDt = targetTime - currentTime;
      }

      timer.tick();

      // -------------------------------------------------------------
      // Generic Explicit Euler Pipeline
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

      // 显式欧拉每一步都是真实的物理时间推进，拓扑必须实时更新
      ctx.setIncrementStart(true);
      ctx.setCurrentDt(currentDt); // 同步真实 dt 给接触模块
      ctx.setCurrentTime(currentTime); // 同步真实总时间给材料模块
      evaluateForces(ctx, kernels, rateFieldNames_, config.stepId, activeLF);

      updateKinematicsEuler(currentDt);

      BC::applyConstraints(ctx.getBCManager(), activeLF, config.stepId);

      for (auto &kernel : kernels) {
        kernel->postCompute(ctx);
      }

      // [状态递进]
      ctx.getFieldManager().executeAllRegisteredSwaps();
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
      "  |  Pure Compute: " + PDCommon::Utils::StringUtils::toScientific(timer.pureComputeTime()) +
      "s" + "  |  Total: " + PDCommon::Utils::StringUtils::toScientific(timer.totalElapsed()) + "s" +
      "  |  Speed: " +
      std::to_string(static_cast<int>(timer.pureSpeed())) + " steps/s");
  if (outputCallback) {
    outputCallback(globalStepCounter, currentTime);
  }

  timer.logSummary("ExplicitEuler");
}

void ExplicitEuler::updateKinematicsEuler(double dt) {
  for (auto &fp : foTargets_) {
#pragma omp parallel for schedule(static)
    for (int i = 0; i < static_cast<int>(fp.totalComponents); ++i) {
      fp.primaryPtr[i] += fp.ratePtr[i] * dt;
    }
  }
}

double ExplicitEuler::computeCFLTimestep(PDCommon::Core::PDContext &ctx,
                                         double massScale, double safetyFactor) {
  auto &matManager = ctx.getMaterialManager();
  auto &fieldManager = ctx.getFieldManager();

  auto *volumeField = fieldManager.getFieldAs<double>("Volume");
  if (!volumeField) {
    LOG_WARNING("[ExplicitEuler] Volume field not found, cannot auto-calc dt. "
                "Returning current dt.");
    return dt_;
  }

  const double *volumes = volumeField->dataPtr();
  if (volumes[0] <= 0.0)
    return dt_;
  double dx = (ctx.getDimension() == 2)
                  ? std::sqrt(volumes[0] / ctx.getThickness())
                  : std::cbrt(volumes[0]);

  // 取最苛刻的热扩散 dt
  double minDt = 1e30;
  bool foundThermal = false;

  for (const auto &[name, matPtr] : matManager.getMaterials()) {
    auto *thermalMat =
        dynamic_cast<PDCommon::Material::ThermalMaterial *>(matPtr.get());
    if (thermalMat) {
      double k = thermalMat->getConductivity();
      double rho = thermalMat->getDensity();
      double cp = thermalMat->getHeatCapacity();
      if (rho > 1e-30 && cp > 1e-30 && k > 0.0) {
        double alpha = k / (rho * cp);
        // 对于三维扩散，临界阻尼步长安全限度通常约为 dx^2 / (6 * alpha)。
        double dt_limit = (dx * dx) / (6.0 * alpha);
        if (dt_limit < minDt) {
          minDt = dt_limit;
          foundThermal = true;
        }
      }
    }
  }

  if (foundThermal) {
    double limitDt = safetyFactor * minDt;
    LOG_INFO("[" + getName() + "] Computed safe Thermal limit dt: " + PDCommon::Utils::StringUtils::toScientific(limitDt) +
             " (dx = " + PDCommon::Utils::StringUtils::toScientific(dx) + ", minDt = " + PDCommon::Utils::StringUtils::toScientific(minDt) + ")");
    return limitDt;
  } else {
    // 如果没有热场且运行了 ExplicitEuler（比如耗散性的力学阻尼问题），
    // 降级退回给基类的力学 CFL 波速计算。
    return TimeIntegrator::computeCFLTimestep(ctx, massScale, safetyFactor);
  }
}

} // namespace Src::Integration

#include "TimeIntegratorRegistry.h"
REGISTER_TIME_INTEGRATOR(Explicit, Src::Integration::ExplicitEuler)
