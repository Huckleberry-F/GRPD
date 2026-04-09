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
#include "Timer.h"
#include <cmath>
#include <omp.h>

namespace Src::Integration {

void ADR_Integrator::configure(const YAML::Node &solverNode) {
  TimeIntegrator::configure(solverNode);

  if (solverNode["MaxPseudoSteps"])
    maxPseudoSteps_ = solverNode["MaxPseudoSteps"].as<int>();
  if (solverNode["DispTol"])
    dispTol_ = solverNode["DispTol"].as<double>();
  if (solverNode["ForceTol"])
    forceTol_ = solverNode["ForceTol"].as<double>();
  if (solverNode["MassScaleFactor"])
    massScaleFactor_ = solverNode["MassScaleFactor"].as<double>();
  if (solverNode["RampIters"])
    rampItersOverride_ = solverNode["RampIters"].as<int>();
  if (solverNode["RampWaveRatio"])
    rampWaveRatio_ = solverNode["RampWaveRatio"].as<double>();
  if (solverNode["DampingMethod"])
    dampingMethod_ = solverNode["DampingMethod"].as<std::string>();
  if (solverNode["Fracture_Strategy"])
    fractureStrategy_ = solverNode["Fracture_Strategy"].as<std::string>();
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
           std::to_string(loadStepConfigs_.size()) +
           ", MassScale = " + std::to_string(massScaleFactor_) +
           ", DampingMethod = " + dampingMethod_ +
           ", dt = " + std::to_string(dt));

  PDCommon::Utils::Timer timer;
  timer.start();

  // [初始状态输出] 开始迭代前，先输出一次初始未变形装配，便于后处理参考对比
  int globalSubstepCounter = 0;
  if (outputCallback) {
    outputCallback(globalSubstepCounter, 0.0);
  }

  // 2. 积分主循环
  for (size_t stepIdx = 0; stepIdx < loadStepConfigs_.size(); ++stepIdx) {
    const auto& stepConfig = loadStepConfigs_[stepIdx];
    int step = stepConfig.stepId - 1; // mapping to internal index for logging
    int currentNumSubsteps = stepConfig.numSubsteps;
    int currentKbc = (stepConfig.kbc >= 0) ? stepConfig.kbc : kbc_;

    LOG_INFO("=== Load Step " + std::to_string(stepConfig.stepId) + " / " +
             std::to_string(loadStepConfigs_.size()) + 
             " | NumSubsteps: " + std::to_string(currentNumSubsteps) + 
             " | KBC: " + std::to_string(currentKbc) + " ===");

    for (int sub = 0; sub < currentNumSubsteps; ++sub) {
      saveBaseDisplacement();

      // =======================================================================
      // 断裂稳定化机制分支 (Dual-Strategy Fracture Logic)
      // =======================================================================
      bool fractureStabilized = false;
      int fracIter = 0;
      // Staggered 模式最多重启15次；FastInnerLoop 模式仅进行 1 次外层循环
      const int MAX_FRAC_ITERS = (fractureStrategy_ == "Staggered") ? 15 : 1; 

      while (!fractureStabilized && fracIter < MAX_FRAC_ITERS) {
        fractureStabilized = true;

      // [加载因子控制] 根据各自的子步数计算全局或局部LF
      double targetLF = (static_cast<double>(stepIdx) +
                         static_cast<double>(sub + 1) / currentNumSubsteps) /
                        loadStepConfigs_.size();
      double prevLF = (static_cast<double>(stepIdx) +
                       static_cast<double>(sub) / currentNumSubsteps) /
                      loadStepConfigs_.size();

      int currentKbc = (stepConfig.kbc >= 0) ? stepConfig.kbc : kbc_;
      int rampIters = (currentKbc == 1) ? 0 : computeRampIters(ctx); // KBC=1阶跃
      LOG_INFO("    [Substep " + std::to_string(sub) +
               "] Computed RampIters: " + std::to_string(rampIters));

      bool converged = false;
      int iter = 0;

      while (!converged && iter < maxPseudoSteps_) {
        bool inRampPhase = (iter < rampIters);
        double currentLF = targetLF;
        
        double baseLocalLF = static_cast<double>(sub + 1) / currentNumSubsteps; 
        double prevLocalLF = static_cast<double>(sub) / currentNumSubsteps;
        double currentLocalLF = baseLocalLF;

        if (inRampPhase) {
          currentLF = prevLF + (targetLF - prevLF) *
                                   (static_cast<double>(iter + 1) / rampIters);
          currentLocalLF = prevLocalLF + (baseLocalLF - prevLocalLF) * 
                                         (static_cast<double>(iter + 1) / rampIters);
        }
        
        double activeLF = (currentKbc == 1) ? 1.0 : currentLocalLF;

        saveOldDisplacement();

        timer.tick();

        // 💡 [核心工序流水线表达]
        bcManager.applyConstraints(activeLF, stepConfig.stepId);
        evaluateForces(ctx, kernels, accFieldNames_);
        bcManager.applyConstraints(activeLF, stepConfig.stepId); // 重施约束斩断支座虚假加速度

        // 【混合加速断裂机制分支】：若开启 FastInnerLoop，则在内存循环中做极速判定
        if (fractureStrategy_ == "FastInnerLoop") {
            if (iter > rampIters + 20 && iter % 50 == 0) {
                for (auto &kernel : kernels) {
                    kernel->postCompute(ctx); // 立刻断键，利用余下步骤阻尼自然耗散
                }
            }
        }

        double cn = computeAdaptiveDamping(dt); // 依据响应提取耗散系数
        updateKinematicsLeapfrog(cn, dt);       // 积分器速度/位移步进推演

        bcManager.applyConstraints(activeLF, stepConfig.stepId); // 始终强制冻结位移边界

        computeConvergenceCriteria(); // 收集收敛 TOL 数据

        timer.tock();

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
          double prevPhysicalTime = (stepIdx == 0) ? 0.0 : loadStepConfigs_[stepIdx - 1].targetTime;
          double stepPhysicalTime = stepConfig.targetTime;
          double currentPhysicalTime = prevPhysicalTime + currentLocalLF * (stepPhysicalTime - prevPhysicalTime);

          LOG_INFO(
              "    > Sub " + std::to_string(sub) + " Iter " +
              std::to_string(iter) + " | Time: " + std::to_string(currentPhysicalTime) +
              " | LocalLF: " + std::to_string(currentLocalLF) +
              " | Res: " + PDCommon::Utils::StringUtils::toScientific(TOL3_) +
              " | cn: " + PDCommon::Utils::StringUtils::toScientific(cn) +
              " | Speed: " + std::to_string(static_cast<int>(timer.pureSpeed())) +
              " steps/s" + (inRampPhase ? " [Ramping]" : ""));
        }

        iter++;
        isFirstExplicitTick_ = false;
      }

      if (!converged) {
        LOG_WARNING(
            "    [Step " + std::to_string(step) + " / Sub " +
            std::to_string(sub) +
            "] Reached MaxPseudoSteps without convergence!" +
            " | TOL1(V): " + PDCommon::Utils::StringUtils::toScientific(TOL1_) +
            " | TOL2(dU): " +
            PDCommon::Utils::StringUtils::toScientific(TOL2_) +
            " | TOL3(Res): " +
            PDCommon::Utils::StringUtils::toScientific(TOL3_));
      }

      if (fractureStrategy_ == "Staggered") {
        // [物理场后处理演算 (含断裂发生)] - 严格交错机制下在静力平衡后执行
        auto *activeStatusField = ctx.getFieldManager().getFieldAs<int>("ActiveStatus");
        int healthBefore = 0;
        if (activeStatusField) {
          const int *ptr = activeStatusField->dataPtr();
          int numParticles = ctx.getParticleManager().getTotalParticles();
          for (int i = 0; i < numParticles; ++i) healthBefore += ptr[i];
        }

        for (auto &kernel : kernels) {
          kernel->postCompute(ctx);
        }

        int healthAfter = 0;
        if (activeStatusField) {
          const int *ptr = activeStatusField->dataPtr();
          int numParticles = ctx.getParticleManager().getTotalParticles();
          for (int i = 0; i < numParticles; ++i) healthAfter += ptr[i];
        }

        if (healthAfter < healthBefore) {
          fractureStabilized = false;
          LOG_INFO("    >> [*FRACTURE*] " + std::to_string(healthBefore - healthAfter) + 
                   " points fractured in quasistatic equilibrium! Restarting pseudo-relaxation to dissipate crack energy (FracIter: " + 
                   std::to_string(fracIter + 1) + ").");
          
          isFirstExplicitTick_ = true;
        }
      } else {
        // FastInnerLoop: 外层仅走1次，此处做最后一次保底收尾
        for (auto &kernel : kernels) {
          kernel->postCompute(ctx);
        }
      }
      
      fracIter++;
    } // 结束 Staggered / FastInner 外层循环

      // [状态递进]
      for (auto &[matName, matPtr] : ctx.getMaterialManager().getMaterials()) {
        if (matPtr)
          matPtr->commitState();
      }

      if (outputCallback) {
        globalSubstepCounter++;
        double prevPhysicalTime = (stepIdx == 0) ? 0.0 : loadStepConfigs_[stepIdx - 1].targetTime;
        double stepPhysicalTime = stepConfig.targetTime;
        double substepFraction = static_cast<double>(sub + 1) / currentNumSubsteps;
        double currentPhysicalTime = prevPhysicalTime + substepFraction * (stepPhysicalTime - prevPhysicalTime);
        outputCallback(globalSubstepCounter, currentPhysicalTime);
      }
    }

    // 宣告本 LoadStep 彻底结束，让 Boundary Conditions 记忆最终态
    bcManager.commitEndStep();
  }

  timer.logSummary("ADR_Integrator");
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
  // 纯动能法：完全弃用粘性阻尼系数（返回 cn = 0）
  // 注意：HybridKinetic 会继续执行下去，计算全局自适应的 cn
  if (dampingMethod_ == "LocalKinetic" || dampingMethod_ == "GlobalKinetic") {
    return 0.0;
  }
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
        // [关键重构]：彻底抛弃极度不稳定的“单自由度局部差分法” (dU*dU * da /
        // dv)。 改用绝对鲁棒的能量法瑞利商 (Rayleigh Quotient) 进行全场积分：
        // 修正：dispOld_ 在循环开头就已经被覆盖为了当前的 uPtr，所以相减是 0！
        // 真正的上一子步真实位移增量，完全等价于最后一次使用的更新速度 * dt：
        double dU_step = velHalfOld_[k][i] * dt;
        double da = so.aPtr[i] - aOld_[k][i]; // 对应的加速度变化率

        local_cn1 -= dU_step * da;
        local_cn2 += dU_step * dU_step;
      }
      cn1 += local_cn1;
      cn2 += local_cn2;
    }
  }

  if (cn2 > 1e-30 && (cn1 / cn2) > 0.0) {
    cn = 2.0 * std::sqrt(cn1 / cn2);
  }
  if (cn > 1.8 / dt)
    cn = 1.8 / dt;

  // // 持久化保护：如果计算得到的 cn 无效
  // // (例如由于全场速度被斩波归零，或者发生刚体漂移)， 使用上一次有效的
  // // cn，保持粘性阻尼在动能被斩波期间的连续吸能能力
  // if (cn > 0.0) {
  //   lastValidCn_ = cn;
  // } else {
  //   cn = lastValidCn_;
  // }

  return cn;
}

void ADR_Integrator::updateKinematicsLeapfrog(double cn, double dt) {
  const bool useLocalKinetic = (dampingMethod_ == "LocalKinetic");
  const bool useGlobalPeakCheck =
      (dampingMethod_ == "GlobalKinetic" || dampingMethod_ == "HybridKinetic");

  bool globalPeakDetected = false;

  // ================================================================
  // 全局/混合阻尼核心：提前预结算真实的全局功率 (Global Power)
  // 当 全局功率 < 0 时，说明全系统总动能刚刚越过峰值，应当同时进行速度斩波
  // ================================================================
  if (useGlobalPeakCheck && !isFirstExplicitTick_) {
    double globalPower = 0.0;
    for (size_t k = 0; k < soTargets_.size(); ++k) {
      const auto &so = soTargets_[k];
      double localPower = 0.0;
#pragma omp parallel for reduction(+ : localPower) schedule(static)
      for (int i = 0; i < static_cast<int>(so.totalComponents); ++i) {
        // 使用实际生效的 cn 计算预期的下一步速度
        double velNew =
            ((2.0 - cn * dt) * velHalfOld_[k][i] + 2.0 * dt * so.aPtr[i]) /
            (2.0 + cn * dt);
        double vMid = 0.5 * (velHalfOld_[k][i] + velNew);
        localPower += vMid * so.aPtr[i];
      }
      globalPower += localPower;
    }
    if (globalPower < 0.0) {
      globalPeakDetected = true;
    }
  }

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

      // ================================================================
      // 动能法斩波：
      // 1. Local模式: 每个粒子自行查自身功率
      // 2. Global/Hybrid模式:听从全局信号齐步走
      // 注意：一旦触发，立刻将本该很大的 velHalfNew
      // 强制重置为从零再加速的一小段速度
      // ================================================================
      if (useLocalKinetic && !isFirstExplicitTick_) {
        double vMid = 0.5 * (velHalfOld_[k][i] + velHalfNew);
        if (vMid * so.aPtr[i] < 0.0) {
          velHalfNew = 0.5 * dt * so.aPtr[i];
        }
      } else if (useGlobalPeakCheck && globalPeakDetected) {
        // 全局同时降速，保留模式形态不散掉
        velHalfNew = 0.5 * dt * so.aPtr[i];
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
  double denom_TOL2 = 0.0;

  for (size_t k = 0; k < soTargets_.size(); ++k) {
    const auto &so = soTargets_[k];
    double local_tol1 = 0.0;
    double local_tol2 = 0.0;
    double local_tol3 = 0.0;
    double local_denom_tol2 = 0.0;
#pragma omp parallel for reduction(+ : local_tol1, local_tol2, local_tol3,     \
                                       local_denom_tol2) schedule(static)
    for (int i = 0; i < static_cast<int>(so.totalComponents); ++i) {
      double du_iter = so.uPtr[i] - dispOld_[k][i];
      double du_substep = so.uPtr[i] - dispBase_[k][i];

      local_tol1 += velHalfOld_[k][i] * velHalfOld_[k][i];
      local_tol2 += du_iter * du_iter;
      local_denom_tol2 += du_substep * du_substep;
      local_tol3 += so.aPtr[i] * so.aPtr[i];
    }
    TOL1_ += local_tol1;
    TOL2_ += local_tol2;
    denom_TOL2 += local_denom_tol2;
    TOL3_ += local_tol3;
  }

  TOL1_ = std::sqrt(TOL1_);
  TOL2_ = std::sqrt(TOL2_);
  denom_TOL2 = std::sqrt(denom_TOL2);
  TOL3_ = std::sqrt(TOL3_);

  // 使用相对位移进行 TOL2 判定（防除零保护）
  if (denom_TOL2 > 1e-12) {
    TOL2_ = TOL2_ / denom_TOL2;
  }
}

} // namespace Src::Integration

REGISTER_TIME_INTEGRATOR(ADR, Src::Integration::ADR_Integrator)
