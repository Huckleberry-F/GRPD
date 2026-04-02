// ============================================================================
// ExplicitEuler.cpp - 显式欧拉时间积分器实现（多核协同版本）
// ============================================================================

#include "ExplicitEuler.h"
#include "BCManager.h"
#include "FieldManager.h"
#include "Logger.h"
#include "MaterialManager.h"
#include "ThermalMaterial.h"
#include "PDKernel.h"
#include <chrono>
#include <cmath>
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
    // 调用继承的虚拟方法，现在它被多态覆盖以处理热计算
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

void ExplicitEuler::computeCFLTimestep(PDCommon::Core::PDContext &ctx,
                                       double massScale,
                                       double safetyFactor) {
  auto &matManager = ctx.getMaterialManager();
  auto &fieldManager = ctx.getFieldManager();

  auto *volumeField = fieldManager.getFieldAs<double>("Volume");
  if (!volumeField) {
    LOG_WARNING("[ExplicitEuler] Volume field not found, cannot auto-calc dt. "
                "Using dt = " + std::to_string(dt_));
    return;
  }

  const double *volumes = volumeField->dataPtr();
  if (volumes[0] <= 0.0) return;
  double dx = std::cbrt(volumes[0]);

  // 取最苛刻的热扩散 dt
  double minDt = 1e30;
  bool foundThermal = false;

  for (const auto &[name, matPtr] : matManager.getMaterials()) {
    auto *thermalMat = dynamic_cast<PDCommon::Material::ThermalMaterial *>(matPtr.get());
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
    dt_ = safetyFactor * minDt;
    LOG_INFO("[" + getName() + "] Auto Thermal dt: dx = " + std::to_string(dx) +
             ", dt_limit = " + std::to_string(minDt) +
             ", final dt = " + std::to_string(dt_));
  } else {
    // 如果没有热场且运行了 ExplicitEuler（比如耗散性的力学阻尼问题），
    // 降级退回给基类的力学 CFL 波速计算。
    TimeIntegrator::computeCFLTimestep(ctx, massScale, safetyFactor);
  }
}

} // namespace Src::Integration

#include "TimeIntegratorRegistry.h"
REGISTER_TIME_INTEGRATOR(Explicit, Src::Integration::ExplicitEuler)
