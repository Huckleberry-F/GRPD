// ============================================================================
// ADR_Integrator.cpp - 自适应动态松弛 / 显式准静态积分器 (Leapfrog/Verlet 格式)
// ============================================================================

#include "ADR_Integrator.h"
#include "BCManager.h"
#include "FieldManager.h"
#include "Logger.h"
#include "MaterialManager.h"
#include "MechanicalMaterial.h"
#include "PDKernel.h"
#include "ParticleManager.h"
#include "StringUtils.h"
#include "TimeIntegratorRegistry.h"
#include <cmath>
#include <omp.h>

namespace Src::Integration {

void ADR_Integrator::configure(const YAML::Node &solverNode) {
  TimeIntegrator::configure(solverNode);

  if (solverNode["NumLoadSteps"])
    numLoadSteps_ = solverNode["NumLoadSteps"].as<int>();
  if (solverNode["NumSubsteps"])
    numSubsteps_ = solverNode["NumSubsteps"].as<int>();
  if (solverNode["KBC"])
    kbc_ = solverNode["KBC"].as<int>();
  if (solverNode["MaxPseudoSteps"])
    maxPseudoSteps_ = solverNode["MaxPseudoSteps"].as<int>();
  if (solverNode["DispTol"])
    dispTol_ = solverNode["DispTol"].as<double>();
  if (solverNode["ForceTol"])
    forceTol_ = solverNode["ForceTol"].as<double>();
  if (solverNode["MassScaleFactor"])
    massScaleFactor_ = solverNode["MassScaleFactor"].as<double>();
  if (solverNode["RampWaveRatio"])
    rampWaveRatio_ = solverNode["RampWaveRatio"].as<double>();
  if (solverNode["RampIters"])
    rampItersOverride_ = solverNode["RampIters"].as<int>();
}

// ---------------------------------------------------------------------------
// computeRampIters：基于真实波速自动计算内层平滑爬坡迭代步数
// ---------------------------------------------------------------------------
int ADR_Integrator::computeRampIters(PDCommon::Core::PDContext &ctx) {
  if (rampItersOverride_ > 0)
    return rampItersOverride_;

  auto &matManager = ctx.getMaterialManager();
  auto &fieldManager = ctx.getFieldManager();
  auto *volumeField = fieldManager.getFieldAs<double>("Volume");
  if (!volumeField)
    return 1000;

  const size_t numParticles = ctx.getParticleManager().getTotalParticles();
  if (numParticles == 0)
    return 1000;

  double dx = std::cbrt(volumeField->dataPtr()[0]);
  double L_char = dx * std::cbrt(static_cast<double>(numParticles));

  double maxWaveSpeed = 0.0;
  for (const auto &[name, matPtr] : matManager.getMaterials()) {
    auto *mechMat =
        dynamic_cast<PDCommon::Material::MechanicalMaterial *>(matPtr.get());
    if (mechMat) {
      double E = mechMat->getYoungsModulus();
      double rho = mechMat->getDensity();
      if (rho > 1e-30 && E > 0.0) {
        // [修复] 用户指示：使用质量缩放后的有效波速
        double rho_eff = rho * massScaleFactor_;
        double c_eff = std::sqrt(E / rho_eff);
        if (c_eff > maxWaveSpeed) {
          maxWaveSpeed = c_eff;
        }
      }
    }
  }

  if (maxWaveSpeed < 1e-30 || dt_ < 1e-30)
    return 1000;

  return static_cast<int>(
      std::ceil(rampWaveRatio_ * L_char / (maxWaveSpeed * dt_)));
}

// ============================================================================
// run() - ADR 自文档化顶层重构
// ============================================================================
void ADR_Integrator::run(PDCommon::Core::PDContext &ctx,
                         std::vector<std::unique_ptr<PDKernel>> &kernels,
                         std::function<void(int, double)> outputCallback) {

  auto &bcManager = ctx.getBCManager();

  // 1. 获取物理关联并分配历史信息
  extractSecondOrderTargets(kernels, ctx, soTargets_, accFieldNames_);
  if (soTargets_.empty()) {
    LOG_ERROR("[ADR_Integrator] No 2nd-order ODE pairs found! Cannot run ADR.");
    return;
  }

  for (auto &kernel : kernels) {
    kernel->setMassScaleFactor(massScaleFactor_);
    kernel->preCompute(ctx);
  }

  if (autoCalcDt_) {
    computeCFLTimestep(ctx, massScaleFactor_);
  }

  const double dt = dt_;
  initializeHistoryVariables();

  LOG_INFO("[ADR_Integrator] Starting Custom ADR: NumLoadSteps = " +
           std::to_string(numLoadSteps_) + ", NumSubsteps = " +
           std::to_string(numSubsteps_) + ", KBC = " + std::to_string(kbc_) +
           ", MassScale = " + std::to_string(massScaleFactor_) +
           ", dt = " + std::to_string(dt));

  // 2. 积分主循环
  for (int step = 0; step < numLoadSteps_; ++step) {
    LOG_INFO("=== Load Step " + std::to_string(step) + " / " +
             std::to_string(numLoadSteps_ - 1) + " ===");

    for (int sub = 0; sub < numSubsteps_; ++sub) {
      saveBaseDisplacement();

      // [加载因子控制]
      double targetLF = (static_cast<double>(step) +
                         static_cast<double>(sub + 1) / numSubsteps_) /
                        numLoadSteps_;
      double prevLF = (static_cast<double>(step) +
                       static_cast<double>(sub) / numSubsteps_) /
                      numLoadSteps_;

      int rampIters = (kbc_ == 1) ? 0 : computeRampIters(ctx); // KBC=1阶跃
      LOG_INFO("    [Substep " + std::to_string(sub) + "] Computed RampIters: " + std::to_string(rampIters));

      bool converged = false;
      int iter = 0;

      while (!converged && iter < maxPseudoSteps_) {
        bool inRampPhase = (iter < rampIters);
        double currentLF = targetLF;

        if (inRampPhase) {
          currentLF = prevLF + (targetLF - prevLF) *
                                   (static_cast<double>(iter + 1) / rampIters);
        }

        saveOldDisplacement();

        // 💡 [核心工序流水线表达]
        bcManager.applyConstraints(currentLF);
        evaluateForces(ctx, kernels, accFieldNames_);
        bcManager.applyConstraints(currentLF); // 重施约束斩断支座虚假加速度

        double cn = computeAdaptiveDamping(dt); // 依据响应提取耗散系数
        updateKinematicsLeapfrog(cn, dt);       // 积分器速度/位移步进推演

        bcManager.applyConstraints(currentLF); // 始终强制冻结位移边界

        for (auto &kernel : kernels) {
          kernel->postCompute(ctx);
        }

        computeConvergenceCriteria(); // 收集收敛 TOL 数据

        // [收敛判定与日志输出]
        if (!inRampPhase && iter > rampIters + 20) {
          if (TOL2_ < dispTol_ && TOL3_ < forceTol_) {
            converged = true;
            LOG_INFO(
                "    [Step " + std::to_string(step) + " / Sub " +
                std::to_string(sub) +
                "] Converged. Iter: " + std::to_string(iter) + " | TOL1(V): " +
                PDCommon::Utils::StringUtils::toScientific(TOL1_) +
                " | TOL2(dU): " +
                PDCommon::Utils::StringUtils::toScientific(TOL2_) +
                " | TOL3(Res): " +
                PDCommon::Utils::StringUtils::toScientific(TOL3_) +
                " | cn: " + PDCommon::Utils::StringUtils::toScientific(cn));
          }
        }

        if (iter % outputInterval_ == 0 && iter > 0) {
          LOG_INFO(
              "    > Step " + std::to_string(step) + " / Sub " +
              std::to_string(sub) + " Iter " + std::to_string(iter) +
              " | LF: " + std::to_string(currentLF) +
              " | TOL1: " + PDCommon::Utils::StringUtils::toScientific(TOL1_) +
              " | TOL2: " + PDCommon::Utils::StringUtils::toScientific(TOL2_) +
              " | TOL3: " + PDCommon::Utils::StringUtils::toScientific(TOL3_) +
              " | cn: " + PDCommon::Utils::StringUtils::toScientific(cn) +
              (inRampPhase ? " [Ramping]" : ""));
        }

        iter++;
        isFirstExplicitTick_ = false;
      }

      if (!converged) {
        LOG_WARNING("    [Step " + std::to_string(step) + " / Sub " +
                    std::to_string(sub) +
                    "] Reached MaxPseudoSteps without convergence!" +
                    " | TOL1(V): " + PDCommon::Utils::StringUtils::toScientific(TOL1_) +
                    " | TOL2(dU): " + PDCommon::Utils::StringUtils::toScientific(TOL2_) +
                    " | TOL3(Res): " + PDCommon::Utils::StringUtils::toScientific(TOL3_));
      }

      // [状态递进]
      for (auto &[matName, matPtr] : ctx.getMaterialManager().getMaterials()) {
        if (matPtr)
          matPtr->commitState();
      }

      if (outputCallback) {
        outputCallback(step * numSubsteps_ + sub + 1, targetLF);
      }
    }
  }
}

// ============================================================================
// 算法拆解实现的私有封装过程
// ============================================================================

void ADR_Integrator::initializeHistoryVariables() {
  velHalfOld_.resize(soTargets_.size());
  aOld_.resize(soTargets_.size());
  dispOld_.resize(soTargets_.size());
  dispBase_.resize(soTargets_.size());

  for (size_t k = 0; k < soTargets_.size(); ++k) {
    velHalfOld_[k].assign(soTargets_[k].totalComponents, 0.0);
    aOld_[k].assign(soTargets_[k].totalComponents, 0.0);
    dispOld_[k].assign(soTargets_[k].totalComponents, 0.0);
    dispBase_[k].assign(soTargets_[k].totalComponents, 0.0);
  }
  isFirstExplicitTick_ = true;
}

void ADR_Integrator::saveBaseDisplacement() {
  for (size_t k = 0; k < soTargets_.size(); ++k) {
    const auto &so = soTargets_[k];
#pragma omp parallel for schedule(static)
    for (int i = 0; i < static_cast<int>(so.totalComponents); ++i) {
      dispBase_[k][i] = so.uPtr[i];
    }
  }
}

void ADR_Integrator::saveOldDisplacement() {
  for (size_t k = 0; k < soTargets_.size(); ++k) {
    const auto &so = soTargets_[k];
#pragma omp parallel for schedule(static)
    for (int i = 0; i < static_cast<int>(so.totalComponents); ++i) {
      dispOld_[k][i] = so.uPtr[i];
    }
  }
}

double ADR_Integrator::computeAdaptiveDamping(double dt) {
  double cn = 0.0;
  double cn1 = 0.0;
  double cn2 = 0.0;

  if (!isFirstExplicitTick_) {
    for (size_t k = 0; k < soTargets_.size(); ++k) {
      const auto &so = soTargets_[k];
      double local_cn1 = 0.0;
      double local_cn2 = 0.0;
#pragma omp parallel for reduction(- : local_cn1) reduction(+ : local_cn2)     \
    schedule(static)
      for (int i = 0; i < static_cast<int>(so.totalComponents); ++i) {
        double dU = so.uPtr[i] - dispBase_[k][i];
        double inv_vhalf = (std::abs(velHalfOld_[k][i]) > 1e-16)
                               ? (1.0 / velHalfOld_[k][i])
                               : 0.0;
        local_cn1 -= dU * dU * (so.aPtr[i] - aOld_[k][i]) * inv_vhalf / dt;
        local_cn2 += dU * dU;
      }
      cn1 += local_cn1;
      cn2 += local_cn2;
    }
  }

  if (cn2 > 1e-30 && (cn1 / cn2) > 0.0)
    cn = 2.0 * std::sqrt(cn1 / cn2);
  if (cn > 1.8 / dt)
    cn = 1.8 / dt;

  return cn;
}

void ADR_Integrator::updateKinematicsLeapfrog(double cn, double dt) {
  for (size_t k = 0; k < soTargets_.size(); ++k) {
    const auto &so = soTargets_[k];
#pragma omp parallel for schedule(static)
    for (int i = 0; i < static_cast<int>(so.totalComponents); ++i) {
      double velHalfNew = 0.0;

      if (isFirstExplicitTick_) {
        velHalfNew = 0.5 * dt * so.aPtr[i];
      } else {
        velHalfNew =
            ((2.0 - cn * dt) * velHalfOld_[k][i] + 2.0 * dt * so.aPtr[i]) /
            (2.0 + cn * dt);
      }

      so.vPtr[i] = 0.5 * (velHalfOld_[k][i] + velHalfNew);
      so.uPtr[i] = so.uPtr[i] + velHalfNew * dt;

      velHalfOld_[k][i] = velHalfNew;
      aOld_[k][i] = so.aPtr[i];
    }
  }
}

void ADR_Integrator::computeConvergenceCriteria() {
  TOL1_ = 0.0;
  TOL2_ = 0.0;
  TOL3_ = 0.0;

  for (size_t k = 0; k < soTargets_.size(); ++k) {
    const auto &so = soTargets_[k];
    double local_tol1 = 0.0;
    double local_tol2 = 0.0;
    double local_tol3 = 0.0;
#pragma omp parallel for reduction(+ : local_tol1, local_tol2, local_tol3)     \
    schedule(static)
    for (int i = 0; i < static_cast<int>(so.totalComponents); ++i) {
      double du = so.uPtr[i] - dispOld_[k][i];
      local_tol1 += velHalfOld_[k][i] * velHalfOld_[k][i];
      local_tol2 += du * du;
      local_tol3 += so.aPtr[i] * so.aPtr[i];
    }
    TOL1_ += local_tol1;
    TOL2_ += local_tol2;
    TOL3_ += local_tol3;
  }

  TOL1_ = std::sqrt(TOL1_);
  TOL2_ = std::sqrt(TOL2_);
  TOL3_ = std::sqrt(TOL3_);
}

} // namespace Src::Integration

REGISTER_TIME_INTEGRATOR(ADR, Src::Integration::ADR_Integrator)
