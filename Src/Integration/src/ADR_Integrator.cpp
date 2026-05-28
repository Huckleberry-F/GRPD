// ============================================================================
// ADR_Integrator.cpp - 自适应动态松弛 / 显式准静态积分器 (Leapfrog/Verlet 格式)
// ============================================================================

#include "ADR_Integrator.h"
#include "FieldManager.h"
#include "PostProcessorManager.h"
#include "Logger.h"
#include "MaterialManager.h"
#include "MechanicalMaterial.h"
#include "PDKernel.h"
#include "ParticleManager.h"
#include "StringUtils.h"
#include "TimeIntegratorRegistry.h"
#include "Timer.h"
#include <Eigen/Dense>
#include <cmath>
#include <omp.h>


namespace Src::Integration {

using PDCommon::BC::BC;

void ADR_Integrator::configure(const YAML::Node &solverNode) {
  TimeIntegrator::configure(solverNode);

  if (solverNode["MaxPseudoSteps"])
    maxPseudoSteps_ = solverNode["MaxPseudoSteps"].as<int>();
  if (solverNode["KineticTol"])
    kineticTol_ = solverNode["KineticTol"].as<double>();
  if (solverNode["MinRefForce"])
    minRefForce_ = solverNode["MinRefForce"].as<double>();
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

  // ADR 初始刚度法外循环参数（可选，有默认值）
  if (solverNode["NRForceTol"])
    NRForceTol_ = solverNode["NRForceTol"].as<double>();
  if (solverNode["NRDispTol"])
    NRDispTol_ = solverNode["NRDispTol"].as<double>();
  if (solverNode["MaxNRSteps"])
    maxNRIters_ = solverNode["MaxNRSteps"].as<int>();
  if (solverNode["NRStateFrozen"])
    NRStateFrozen_ = solverNode["NRStateFrozen"].as<bool>();
  if (solverNode["NRStiffnessType"])
    NRStiffnessType_ = solverNode["NRStiffnessType"].as<std::string>();
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

  double limitDt = computeCFLTimestep(ctx, massScaleFactor_);
  if (autoCalcDt_) {
    dt_ = limitDt;
    LOG_INFO("[ADR_Integrator] Auto CFL enabled. Setting dt = " +
             PDCommon::Utils::StringUtils::toScientific(dt_));
  } else {
    if (dt_ > limitDt) {
      LOG_WARNING("[ADR_Integrator] Global TimeStep_dt (" +
                  PDCommon::Utils::StringUtils::toScientific(dt_) +
                  ") EXCEEDS safe CFL limit (" +
                  PDCommon::Utils::StringUtils::toScientific(limitDt) +
                  ")! Auto-clamping global dt to the safe limit.");
      dt_ = limitDt;
    }
  }

  const double dt = dt_;
  initializeHistoryVariables();
  initializeNodalMass(ctx);

  ctx.setMassScaleFactor(massScaleFactor_);

  LOG_INFO("[ADR_Integrator] Starting Custom ADR: NumLoadSteps = " +
           std::to_string(loadStepConfigs_.size()) + ", MassScale = " +
           PDCommon::Utils::StringUtils::toScientific(massScaleFactor_) +
           ", DampingMethod = " + dampingMethod_ +
           ", dt = " + PDCommon::Utils::StringUtils::toScientific(dt));

  PDCommon::Utils::Timer timer;
  timer.start();

  // [初始状态输出] 开始迭代前，先输出一次初始未变形装配，便于后处理参考对比
  int globalSubstepCounter = 0;
  if (outputCallback) {
    outputCallback(globalSubstepCounter, 0.0);
  }

  // 2. 积分主循环
  for (size_t stepIdx = 0; stepIdx < loadStepConfigs_.size(); ++stepIdx) {
    const auto &stepConfig = loadStepConfigs_[stepIdx];
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
      // 清空 Anderson 加速历史，因为外部载荷子步已更新，平衡方程发生变化
      // =======================================================================
      if (useAndersonAcceleration_) {
        if (aaHistories_.size() != soTargets_.size()) {
          aaHistories_.resize(soTargets_.size());
        }
        for (auto &hist : aaHistories_) {
          hist.disp.clear();
          hist.res.clear();
        }
        aaResidualNormHistory_.clear();
      }

      // =======================================================================
      // ADR 外循环 (支持两种模式)
      // 模式 1 (NRStateFrozen=true):  增量 Modified NR —— 计算 forceCorr_
      //         让内层 ADR 求解增量方程 K0*Δu = R_true
      // 模式 2 (NRStateFrozen=false): 纯显式 ADR —— 直接用真实本构硬算
      // =======================================================================
      bool NRConverged = false;
      int NRIter = 0;
      double fIntTotal = 0.0; // 外循环中计算的全场总内力，子步结束后保存

      while (!NRConverged && NRIter < maxNRIters_) {

        // [保存外循环位移快照] 用于宏观位移残差计算
        saveNROldDisplacement();

        // [传递外循环状态]
        ctx.setOuterIter(NRIter);

        // =================================================================
        // [增量 Modified NR 核心] 计算本构修正力 forceCorr_
        // forceCorr_[k][i] = a_true[i] - a_frozen[i]
        // 在内循环中将其叠加到冻结态加速度上，使 ADR 等效于求解增量方程
        // =================================================================
        if (NRStateFrozen_) {
          // 计算当前子步的加载因子
          double corrLF_local = static_cast<double>(sub + 1) / currentNumSubsteps;
          double corrActiveLF = (currentKbc == 1) ? 1.0 : corrLF_local;

          // Step A: 解冻态力评估 → 获得真实的非线性加速度 a_true
          // stateMode=0 → 完整的径向回退算法，写入 pSTrial_
          ctx.setStateFrozen(false);
          BC::applyConstraints(ctx.getBCManager(), corrActiveLF, stepConfig.stepId);
          evaluateForces(ctx, kernels, accFieldNames_, stepConfig.stepId, corrActiveLF);

          // 暂存真实加速度
          std::vector<std::vector<double>> accTrue(soTargets_.size());
          for (size_t k = 0; k < soTargets_.size(); ++k) {
            const auto &so = soTargets_[k];
            accTrue[k].resize(so.totalComponents);
            #pragma omp parallel for schedule(static)
            for (int i = 0; i < static_cast<int>(so.totalComponents); ++i) {
              accTrue[k][i] = so.aPtr[i];
            }
          }

          // Step B: 冻结态力评估 → 获得初始刚度/切线刚度弹性加速度 a_frozen
          ctx.setStateFrozen(true);
          if (NRStiffnessType_ == "Initial") {
            ctx.setOuterIter(0);  // 强制读取 pSOld_ (stateMode=1)
          } else if (NRStiffnessType_ == "Tangent") {
            ctx.setOuterIter(NRIter); // 根据当前迭代决定 stateMode: 0次为1, 其余为2
          }
          BC::applyConstraints(ctx.getBCManager(), corrActiveLF, stepConfig.stepId);
          evaluateForces(ctx, kernels, accFieldNames_, stepConfig.stepId, corrActiveLF);
          ctx.setOuterIter(NRIter);  // 恢复真实的外循环迭代计数

          // Step C: 计算修正力 = a_true - a_frozen
          for (size_t k = 0; k < soTargets_.size(); ++k) {
            const auto &so = soTargets_[k];
            #pragma omp parallel for schedule(static)
            for (int i = 0; i < static_cast<int>(so.totalComponents); ++i) {
              forceCorr_[k][i] = accTrue[k][i] - so.aPtr[i];
            }
          }

          LOG_INFO("    " + PDCommon::Utils::Colors::CYAN + "[NR-Increment]" +
                   PDCommon::Utils::Colors::RESET + " Computed forceCorr for NR iter " +
                   std::to_string(NRIter + 1));

          // 恢复冻结态用于内循环（内循环弹性刚度基底应与上方评估修正力时保持一致）
          ctx.setStateFrozen(true);
          if (NRStiffnessType_ == "Initial") {
            ctx.setOuterIter(0);  
          } else if (NRStiffnessType_ == "Tangent") {
            ctx.setOuterIter(NRIter); 
          }
        } else {
          // 纯显式模式：不需要修正力，清零 forceCorr_
          for (size_t k = 0; k < soTargets_.size(); ++k) {
            #pragma omp parallel for schedule(static)
            for (int i = 0; i < static_cast<int>(forceCorr_[k].size()); ++i) {
              forceCorr_[k][i] = 0.0;
            }
          }
          ctx.setStateFrozen(false);
        }

        // =======================================================================
        // 断裂稳定化机制分支 (Dual-Strategy Fracture Logic)
        // =======================================================================
        bool fractureStabilized = false;
        int fracIter = 0;
        // Staggered 模式最多重启15次；FastInnerLoop 模式仅进行 1 次外层循环
        const int MAX_FRAC_ITERS = (fractureStrategy_ == "Staggered") ? 15 : 1;

        bool innerConverged = false;

        while (!fractureStabilized && fracIter < MAX_FRAC_ITERS) {
          fractureStabilized = true;

          // [加载因子控制] 根据各自的子步数计算全局或局部LF
          double targetLF =
              (static_cast<double>(stepIdx) +
               static_cast<double>(sub + 1) / currentNumSubsteps) /
              loadStepConfigs_.size();
          double prevLF = (static_cast<double>(stepIdx) +
                           static_cast<double>(sub) / currentNumSubsteps) /
                          loadStepConfigs_.size();

          int currentKbc = (stepConfig.kbc >= 0) ? stepConfig.kbc : kbc_;
          // 核心修复：断裂二次松弛 或 外循环重入时，强制无爬坡直接保载
          int rampIters = (currentKbc == 1 || fracIter > 0 || NRIter > 0)
                              ? 0
                              : computeRampIters(ctx);
          LOG_INFO("    [Substep " + std::to_string(sub + 1) +
                   "] Computed RampIters: " + std::to_string(rampIters));

          int iter = 0;
          maxKineticEnergy_ = 0.0; // Reset peak kinetic energy tracker for each
                                   // new relaxation process

          while (!innerConverged && iter < maxPseudoSteps_) {
            bool inRampPhase = (iter < rampIters);
            double currentLF = targetLF;

            double baseLocalLF =
                static_cast<double>(sub + 1) / currentNumSubsteps;
            double prevLocalLF = static_cast<double>(sub) / currentNumSubsteps;
            double currentLocalLF = baseLocalLF;

            if (inRampPhase) {
              currentLF =
                  prevLF + (targetLF - prevLF) *
                               (static_cast<double>(iter + 1) / rampIters);
              currentLocalLF =
                  prevLocalLF + (baseLocalLF - prevLocalLF) *
                                    (static_cast<double>(iter + 1) / rampIters);
            }

            double activeLF = (currentKbc == 1) ? 1.0 : currentLocalLF;

            saveOldDisplacement();

            timer.tick();

            // 核心控制：告知所有后续流程当前是否是子步(非迭代)的起跑线
            // 若为是，接触模块需要冻结几何拓扑法向等信息，维持本子步内的完全保守力场
            ctx.setIncrementStart(iter == 0);

            // ✨ [核心工序流水线表达]
            BC::applyConstraints(ctx.getBCManager(), activeLF,
                                 stepConfig.stepId);
            ctx.setCurrentDt(dt); // 同步真实 dt 给接触模块
            double prevPhysicalTime_ADR = (stepIdx == 0) ? 0.0 : loadStepConfigs_[stepIdx - 1].targetTime;
            double currentPhysicalTime_ADR = prevPhysicalTime_ADR + currentLocalLF * (stepConfig.targetTime - prevPhysicalTime_ADR);
            ctx.setCurrentTime(currentPhysicalTime_ADR); // 同步准静态进程物理总时间
            evaluateForces(ctx, kernels, accFieldNames_, stepConfig.stepId,
                           activeLF);

            // =============================================================
            // [增量 Modified NR 核心] 叠加本构修正力 forceCorr_
            // 使内循环从 "求解总位移" 变为 "求解残差增量"
            // forceCorr_ 以加速度为单位，直接加到 aPtr 上即可
            // =============================================================
            if (NRStateFrozen_) {
              for (size_t iTarget = 0; iTarget < soTargets_.size(); ++iTarget) {
                const auto &so = soTargets_[iTarget];
                #pragma omp parallel for schedule(static)
                for (int ii = 0; ii < static_cast<int>(so.totalComponents); ++ii) {
                  so.aPtr[ii] += forceCorr_[iTarget][ii];
                }
              }
            }

            // =====================================================================
            // [内循环实时宏观力基准] 在约束斩断支反力之前，提取全场内部合力
            // =====================================================================
            double currentSumFintSq = 0.0;
            auto *damageFieldInner = ctx.getFieldManager().getFieldAs<double>("Damage_Trial");
            const double *damagePtrInner = damageFieldInner ? damageFieldInner->dataPtr() : nullptr;

            for (size_t iTarget = 0; iTarget < soTargets_.size(); ++iTarget) {
              const auto &so = soTargets_[iTarget];
              double localSq = 0.0;
#pragma omp parallel for reduction(+ : localSq) schedule(static)
              for (int i = 0; i < static_cast<int>(so.totalComponents); ++i) {
                int particle_idx = i / so.dimension;
                
                // [死点屏蔽]：若损伤极大，说明刚度丧失，其微弱波动不计入系统总承载力
                if (damagePtrInner && damagePtrInner[particle_idx] > 0.99) {
                  continue;
                }

                double effective_mass =
                    nodalMass_[particle_idx] * massScaleFactor_;
                double f = so.aPtr[i] * effective_mass;
                localSq += f * f;
              }
              currentSumFintSq += localSq;
            }
            double currentFIntTotal = std::sqrt(currentSumFintSq);
            double currentFRef = std::max(
                std::abs(currentFIntTotal - fIntPrevSubstep_), minRefForce_);

            BC::applyConstraints(
                ctx.getBCManager(), activeLF,
                stepConfig.stepId); // 重施约束斩断支座虚假加速度

            // 【混合加速断裂机制分支】：若开启
            // FastInnerLoop，则在内存循环中做极速判定
            if (fractureStrategy_ == "FastInnerLoop") {
              if (iter > rampIters + 20 && iter % 50 == 0) {
                for (auto &kernel : kernels) {
                  kernel->postCompute(
                      ctx); // 立刻断键，利用余下步骤阻尼自然耗散
                }
              }
            }

            // 【显式准静态拓展】：若 NRStateFrozen 为 false，说明用户希望在 ADR
            // 内部实时更新本构（Trial状态）。
            // [修改]：不再在每一微步 Commit State。这意味着每次本构积分都以**上一个收敛子步的真实历史**为起点，
            // 避免了内循环由于震荡产生虚假的塑性累积路径，从而保证了路径无关性。
            // 真实的 commit 留到外循环（NR/Substep）彻底收敛后执行。

            double cn = computeAdaptiveDamping(dt); // 依据响应提取耗散系数
            updateKinematicsLeapfrog(cn, dt);       // 积分器速度/位移步进推演

            BC::applyConstraints(ctx.getBCManager(), activeLF,
                                 stepConfig.stepId); // 始终强制冻结位移边界

            auto *damageFieldIter = ctx.getFieldManager().getFieldAs<double>("Damage_Trial");
            const double *damagePtrIter = damageFieldIter ? damageFieldIter->dataPtr() : nullptr;

            computeConvergenceCriteria(
                currentFRef, damagePtrIter); // 收集收敛 TOL 数据，使用真实的宏观工程残差比

            timer.tock();

            // [收敛判定与日志输出]
            if (!inRampPhase && iter > rampIters + 20) {
              // 全新的纯粹收敛准则：力平衡 + 动能彻底耗散
              bool criteria_converged =
                  (forceRatio_ <= NRForceTol_ && kineticRatio_ <= kineticTol_);

              // 绝对静止防死锁兜底：系统动能比例彻底归零(1e-16)，即使存在数值底噪导致的残差，也强制退出
              bool criteria_still = (kineticRatio_ < 1e-16);

              if (criteria_converged || criteria_still) {
                innerConverged = true;
                LOG_INFO(
                    "    [Step " + std::to_string(step + 1) + " / Sub " +
                    std::to_string(sub + 1) + "] Converged. Iter: " +
                    std::to_string(iter + 1) + " | KineticRatio: " +
                    PDCommon::Utils::StringUtils::toScientific(kineticRatio_) +
                    " | DispRatio: " +
                    PDCommon::Utils::StringUtils::toScientific(dispRatio_) +
                    " | ForceRatio: " +
                    PDCommon::Utils::StringUtils::toScientific(forceRatio_));
              }
            }

            if (iter % outputInterval_ == 0 && iter > 0) {
              double prevPhysicalTime =
                  (stepIdx == 0) ? 0.0
                                 : loadStepConfigs_[stepIdx - 1].targetTime;
              double stepPhysicalTime = stepConfig.targetTime;
              double currentPhysicalTime =
                  prevPhysicalTime +
                  currentLocalLF * (stepPhysicalTime - prevPhysicalTime);

              LOG_INFO(
                  "    > Sub " + std::to_string(sub + 1) + " Iter " +
                  std::to_string(iter) + " | Time: " +
                  PDCommon::Utils::StringUtils::toScientific(
                      currentPhysicalTime) +
                  " | KineticRatio: " +
                  PDCommon::Utils::StringUtils::toScientific(kineticRatio_) +
                  " | DispRatio: " +
                  PDCommon::Utils::StringUtils::toScientific(dispRatio_) +
                  " | ForceRatio: " +
                  PDCommon::Utils::StringUtils::toScientific(forceRatio_) +
                  " | Speed: " +
                  std::to_string(static_cast<int>(timer.pureSpeed())) +
                  " steps/s" + (inRampPhase ? " [Ramping]" : ""));
            }

            iter++;
            isFirstExplicitTick_ = false;
          }

          if (!innerConverged) {
            LOG_WARNING(
                "    [Step " + std::to_string(step + 1) + " / Sub " +
                std::to_string(sub + 1) +
                "] Reached MaxPseudoSteps without convergence!" +
                " | KineticRatio: " +
                PDCommon::Utils::StringUtils::toScientific(kineticRatio_) +
                " | DispRatio: " +
                PDCommon::Utils::StringUtils::toScientific(dispRatio_) +
                " | ForceRatio: " +
                PDCommon::Utils::StringUtils::toScientific(forceRatio_));
          }

          if (fractureStrategy_ == "Staggered") {
            // [物理场后处理演算 (含断裂发生)] - 严格交错机制下在静力平衡后执行
            auto *activeStatusField =
                ctx.getFieldManager().getFieldAs<int>("ActiveStatus");
            int healthBefore = 0;
            if (activeStatusField) {
              const int *ptr = activeStatusField->dataPtr();
              int numParticles = ctx.getParticleManager().getTotalParticles();
              for (int i = 0; i < numParticles; ++i)
                healthBefore += ptr[i];
            }

            for (auto &kernel : kernels) {
              kernel->postCompute(ctx);
            }

            int healthAfter = 0;
            if (activeStatusField) {
              const int *ptr = activeStatusField->dataPtr();
              int numParticles = ctx.getParticleManager().getTotalParticles();
              for (int i = 0; i < numParticles; ++i)
                healthAfter += ptr[i];
            }

            if (healthAfter < healthBefore) {
              fractureStabilized = false;
              LOG_INFO(
                  "    >> [*FRACTURE*] " +
                  std::to_string(healthBefore - healthAfter) +
                  " points fractured in quasistatic equilibrium! Restarting "
                  "pseudo-relaxation to dissipate crack energy (FracIter: " +
                  std::to_string(fracIter + 1) + ").");

              isFirstExplicitTick_ = true;
            }
          } else if (fractureStrategy_ == "FastInnerLoop") {
            // FastInnerLoop: 外层仅走1次，此处做最后一次保底收尾
            for (auto &kernel : kernels) {
              kernel->postCompute(ctx);
            }
          } else if (fractureStrategy_ == "StrictNRFracture" ||
                     fractureStrategy_ == "LaggedNRFracture" ||
                     fractureStrategy_ == "SimpleNRFracture") {
            // [StrictNR / LaggedNR / SimpleNR]
            // 试探阶段完全绕过断裂物理更新，留到 NR 外循环收敛后再评估
            // (LaggedNRFracture通过TrialDamage实时断开受力)
          }

          fracIter++;
        } // 结束 Staggered / FastInner 外层循环

        // =================================================================
        // [纯显式模式快速收敛] 如果 NRStateFrozen_ == false，
        // 内循环已经用真实本构达到了稳态，即 F_ext - F_int_true = 0，
        // 无需外循环校验，直接宣布收敛。
        // =================================================================
        if (!NRStateFrozen_) {
          // =====================================================================
          // [新增] 为了提取正确的全场内力（含支反力）以及运行 PostProcessor Hook
          // 重新解冻并在无约束下评估一次力
          // =====================================================================
          double NRLocalLF = static_cast<double>(sub + 1) / currentNumSubsteps;
          double NRActiveLF = (currentKbc == 1) ? 1.0 : NRLocalLF;

          evaluateForces(ctx, kernels, accFieldNames_, stepConfig.stepId, NRActiveLF);

          double prevPhysicalTime = (stepIdx == 0) ? 0.0 : loadStepConfigs_[stepIdx - 1].targetTime;
          double currentPhysicalTime = prevPhysicalTime + NRLocalLF * (stepConfig.targetTime - prevPhysicalTime);
          ctx.getPostProcessorManager().executePreBCHooks(ctx, currentPhysicalTime, stepConfig.stepId);

          // 计算全场内力范数供下一子步增量基准使用
          double sumFintAllSq = 0.0;
          auto *damageFieldExp = ctx.getFieldManager().getFieldAs<double>("Damage_Trial");
          const double *damagePtrExp = damageFieldExp ? damageFieldExp->dataPtr() : nullptr;

          for (size_t k = 0; k < soTargets_.size(); ++k) {
            const auto &so = soTargets_[k];
            double localSq = 0.0;
            #pragma omp parallel for reduction(+ : localSq) schedule(static)
            for (int i = 0; i < static_cast<int>(so.totalComponents); ++i) {
              int particle_idx = i / so.dimension;
              
              if (damagePtrExp && damagePtrExp[particle_idx] > 0.99) {
                continue;
              }

              double effective_mass = nodalMass_[particle_idx] * massScaleFactor_;
              double f = so.aPtr[i] * effective_mass;
              localSq += f * f;
            }
            sumFintAllSq += localSq;
          }
          fIntTotal = std::sqrt(sumFintAllSq);

          // 重施约束：清零边界节点的加速度，为输出和下一子步做准备
          BC::applyConstraints(ctx.getBCManager(), NRActiveLF, stepConfig.stepId);

          if (innerConverged) {
            // 断裂处理（如有需要）
            for (auto &kernel : kernels) {
              kernel->postCompute(ctx);
            }

            NRConverged = true;
            LOG_INFO("    " + PDCommon::Utils::Colors::MAGENTA + "[Explicit]" +
                     PDCommon::Utils::Colors::RESET +
                     " Inner loop converged -> substep done. (No outer NR needed)");
          } else {
            LOG_WARNING("    [Explicit] Inner loop did NOT converge within MaxPseudoSteps!");
            for (auto &kernel : kernels) {
              kernel->postCompute(ctx);
            }
            NRConverged = true; // 纯显式模式下不重试，接受当前结果
          }
          NRIter++;
          continue; // 跳过后续的 NR 外循环校验代码
        }

        // =================================================================
        // [外循环修正] 解冻本构，做一次完整的非线性力评估（仅 NR 模式）
        // =================================================================
        ctx.setStateFrozen(false);

        // 加载因子计算（复用当前子步的目标值）
        double NRTargetLF =
            (static_cast<double>(stepIdx) +
             static_cast<double>(sub + 1) / currentNumSubsteps) /
            loadStepConfigs_.size();
        double NRLocalLF = static_cast<double>(sub + 1) / currentNumSubsteps;
        double NRActiveLF = (currentKbc == 1) ? 1.0 : NRLocalLF;

        // 以解冻状态（完整非线性本构）重新评估内力
        BC::applyConstraints(ctx.getBCManager(), NRActiveLF, stepConfig.stepId);
        evaluateForces(ctx, kernels, accFieldNames_, stepConfig.stepId,
                       NRActiveLF);

        // =====================================================================
        // [ANSYS 宏观力准则 Step 1] 在 BC 清零边界加速度之前，
        // 统计全场（含边界节点支反力）的力 L2 范数作为结构总承载力参考基准
        // =====================================================================
        double sumFintAllSq = 0.0;
        auto *damageFieldNR = ctx.getFieldManager().getFieldAs<double>("Damage_Trial");
        const double *damagePtrNR = damageFieldNR ? damageFieldNR->dataPtr() : nullptr;

        for (size_t k = 0; k < soTargets_.size(); ++k) {
          const auto &so = soTargets_[k];
          double localSq = 0.0;
#pragma omp parallel for reduction(+ : localSq) schedule(static)
          for (int i = 0; i < static_cast<int>(so.totalComponents); ++i) {
            int particle_idx = i / so.dimension;
            
            if (damagePtrNR && damagePtrNR[particle_idx] > 0.99) {
              continue;
            }

            double effective_mass = nodalMass_[particle_idx] * massScaleFactor_;
            double f = so.aPtr[i] * effective_mass;
            localSq += f * f;
          }
          sumFintAllSq += localSq;
        }
        fIntTotal = std::sqrt(sumFintAllSq);

        // [关键] 使用本子步的内力增量作为参考基准，而非全场总内力
        // 避免载荷步越多、总力越大时判定过度宽松
        double fIntIncrement = std::abs(fIntTotal - fIntPrevSubstep_);

        // =====================================================================
        // [PostProcessing Hook] 在加速度被清零前，截获最原始的支反力等
        // =====================================================================
        double prevPhysicalTime = (stepIdx == 0) ? 0.0 : loadStepConfigs_[stepIdx - 1].targetTime;
        double currentPhysicalTime = prevPhysicalTime + NRLocalLF * (stepConfig.targetTime - prevPhysicalTime);
        ctx.getPostProcessorManager().executePreBCHooks(ctx, currentPhysicalTime, stepConfig.stepId);

        // 重施约束：清零边界节点的加速度，仅保留自由粒子残差
        BC::applyConstraints(ctx.getBCManager(), NRActiveLF, stepConfig.stepId);

        // =====================================================================
        // [ANSYS 宏观力准则 Step 2] 计算宏观力残差比 + 宏观位移残差比
        // =====================================================================
        double macroForceRatio = 0.0, macroDispRatio = 0.0;
        computeNRConvergence(fIntIncrement, macroForceRatio, macroDispRatio, damagePtrNR);

        bool forceConverged = (macroForceRatio <= NRForceTol_);
        bool dispConverged = (macroDispRatio <= NRDispTol_);

        // 【修正】如果在 NR 的第一轮 (NRIter ==
        // 0)，由于本次迭代的位移恰等于本子步的总位移， macroDispRatio
        // 必然等于 1.0。因此在第一轮只要力收敛即可；或者当力极其收敛时豁免位移检查。
        bool strictConverged = forceConverged && dispConverged;
        bool waiveDispConverged =
            forceConverged &&
            (NRIter == 0 || macroForceRatio <= 0.1 * NRForceTol_);

        // 【核心修复】外循环收敛的绝对前提是：内循环本身必须已经收敛！
        // 如果内循环是因为跑满 MaxPseudoSteps 而超时的，说明动能并未彻底耗散，
        // 此时测到的极小残差可能是系统在振荡过程中恰好穿过平衡点造成的“假象”。
        // 因此，如果 innerConverged ==
        // false，外循环绝对不能放行，必须强制触发下一轮 NR 迭代。
        if (innerConverged && (strictConverged || waiveDispConverged)) {
          // =====================================================================
          // [StrictNRFracture 策略核心]
          // 收敛后拦截，执行试探性断裂并决策是否重启 NR
          // =====================================================================
          if (fractureStrategy_ == "StrictNRFracture") {
            auto *activeStatusField =
                ctx.getFieldManager().getFieldAs<int>("ActiveStatus");
            int healthBefore = 0;
            if (activeStatusField) {
              const int *ptr = activeStatusField->dataPtr();
              int numParticles = ctx.getParticleManager().getTotalParticles();
              for (int i = 0; i < numParticles; ++i)
                healthBefore += ptr[i];
            }

            for (auto &kernel : kernels) {
              kernel->postCompute(ctx);
            }

            int healthAfter = 0;
            if (activeStatusField) {
              const int *ptr = activeStatusField->dataPtr();
              int numParticles = ctx.getParticleManager().getTotalParticles();
              for (int i = 0; i < numParticles; ++i)
                healthAfter += ptr[i];
            }

            if (healthAfter < healthBefore) {
              NRConverged = false;
              LOG_INFO(
                  "    >> " + PDCommon::Utils::Colors::YELLOW + "[*FRACTURE*]" +
                  PDCommon::Utils::Colors::RESET + " NR Converged, but " +
                  std::to_string(healthBefore - healthAfter) +
                  " bonds broke! Topology changed, restarting NR iteration.");
              NRIter++;
              isFirstExplicitTick_ = true;
              continue;
            }
          } else if (fractureStrategy_ == "LaggedNRFracture" || 
                     fractureStrategy_ == "SimpleNRFracture") {
            // =====================================================================
            // [LaggedNR / SimpleNR 策略核心] 收敛后直接断裂并固化，不重启 NR，直接进入下一步
            // =====================================================================
            for (auto &kernel : kernels) {
              kernel->postCompute(ctx);
            }
          }

          NRConverged = true;
          LOG_INFO("    " + PDCommon::Utils::Colors::MAGENTA + "[NR]" +
                   PDCommon::Utils::Colors::RESET + " Converged at iter " +
                   std::to_string(NRIter + 1) + " | MacroForce: " +
                   PDCommon::Utils::StringUtils::toScientific(macroForceRatio) +
                   " | MacroDisp: " +
                   PDCommon::Utils::StringUtils::toScientific(macroDispRatio) +
                   " | dFref: " +
                   PDCommon::Utils::StringUtils::toScientific(fIntIncrement) +
                   " (Total=" +
                   PDCommon::Utils::StringUtils::toScientific(fIntTotal) + ")");
        } else {
          // =====================================================================
          // [Anderson Acceleration 核心算法]
          // 仅在隐式 Picard 迭代 (NRStateFrozen=true)
          // 且未发生严重断裂退级时生效
          // =====================================================================
          if (useAndersonAcceleration_ && NRStateFrozen_) {
            // 1. 计算当前全场残差 L2 范数，用于安全阀判定
            double currentResNormSq = 0.0;
            for (size_t k = 0; k < soTargets_.size(); ++k) {
              const auto &so = soTargets_[k];
              double localSq = 0.0;
#pragma omp parallel for reduction(+ : localSq) schedule(static)
              for (int i = 0; i < static_cast<int>(so.totalComponents); ++i) {
                double res = so.uPtr[i] - dispNROld_[k][i];
                localSq += res * res;
              }
              currentResNormSq += localSq;
            }
            double currentResNorm = std::sqrt(currentResNormSq);

            // 2.
            // 安全阀：如果残差剧烈放大，说明外推过头或非线性跳跃，放弃本次历史
            bool restartAA = false;
            if (!aaResidualNormHistory_.empty()) {
              double prevResNorm = aaResidualNormHistory_.back();
              // [用户定制修改] 放宽残差激增的限制，强制不退回（原系数为 5.0）
              if (currentResNorm > 1e10 * prevResNorm) {
                restartAA = true;
                LOG_INFO("    >> " + PDCommon::Utils::Colors::YELLOW +
                         "[AA Restart]" + PDCommon::Utils::Colors::RESET +
                         " Residual spiked (" + std::to_string(currentResNorm) +
                         " > " + std::to_string(prevResNorm) +
                         "). Clearing Anderson Acceleration history.");
              }
            }

            if (restartAA) {
              for (auto &hist : aaHistories_) {
                hist.disp.clear();
                hist.res.clear();
              }
              aaResidualNormHistory_.clear();
            }

            // 3. 记录当前历史 x_k = dispNROld_, f_k = so.uPtr - dispNROld_
            aaResidualNormHistory_.push_back(currentResNorm);
            for (size_t k = 0; k < soTargets_.size(); ++k) {
              const auto &so = soTargets_[k];
              std::vector<double> currentDisp(so.totalComponents);
              std::vector<double> currentRes(so.totalComponents);
#pragma omp parallel for schedule(static)
              for (int i = 0; i < static_cast<int>(so.totalComponents); ++i) {
                currentDisp[i] = dispNROld_[k][i];
                currentRes[i] = so.uPtr[i] - dispNROld_[k][i];
              }
              aaHistories_[k].disp.push_back(currentDisp);
              aaHistories_[k].res.push_back(currentRes);
              // 维护固定深度
              if (aaHistories_[k].disp.size() > static_cast<size_t>(aaDepth_)) {
                aaHistories_[k].disp.erase(aaHistories_[k].disp.begin());
                aaHistories_[k].res.erase(aaHistories_[k].res.begin());
              }
            }
            if (aaResidualNormHistory_.size() > static_cast<size_t>(aaDepth_)) {
              aaResidualNormHistory_.erase(aaResidualNormHistory_.begin());
            }

            // 4. 当历史积累 >= 2 步时，进行矩阵求逆与加速外推
            int mk = aaHistories_[0].res.size();
            if (mk > 1) {
              Eigen::MatrixXd M = Eigen::MatrixXd::Zero(mk, mk);
              for (int i = 0; i < mk; ++i) {
                for (int j = i; j < mk; ++j) {
                  double dotProduct = 0.0;
                  for (size_t k = 0; k < soTargets_.size(); ++k) {
                    const auto &so = soTargets_[k];
                    double localDot = 0.0;
#pragma omp parallel for reduction(+ : localDot) schedule(static)
                    for (int idx = 0;
                         idx < static_cast<int>(so.totalComponents); ++idx) {
                      localDot += aaHistories_[k].res[i][idx] *
                                  aaHistories_[k].res[j][idx];
                    }
                    dotProduct += localDot;
                  }
                  M(i, j) = dotProduct;
                  M(j, i) = dotProduct;
                }
              }

              // 求解 M * alpha = 1
              Eigen::VectorXd ones = Eigen::VectorXd::Ones(mk);
              Eigen::VectorXd alpha = M.colPivHouseholderQr().solve(ones);
              double sumAlpha = alpha.sum();
              if (std::abs(sumAlpha) > 1e-12) {
                alpha /= sumAlpha; // 强制归一化 sum(alpha) = 1

                // 更新位移 x_{k+1} = sum(alpha_i * (x_{k-i} + f_{k-i}))
                for (size_t k = 0; k < soTargets_.size(); ++k) {
                  const auto &so = soTargets_[k];
#pragma omp parallel for schedule(static)
                  for (int idx = 0; idx < static_cast<int>(so.totalComponents);
                       ++idx) {
                    double beta_AA = 0.3; // [新增] AA松弛因子：控制外推步长，防止塑性屈服时严重过调 (0.2~0.5最佳)
                    double newDisp = 0.0;
                    for (int i = 0; i < mk; ++i) {
                      newDisp += alpha(i) * (aaHistories_[k].disp[i][idx] +
                                             beta_AA * aaHistories_[k].res[i][idx]);
                    }
                    so.uPtr[idx] =
                        newDisp; // 直接将加速后的位移写入，作为下一步的起点
                  }
                }
                LOG_INFO("    " + PDCommon::Utils::Colors::CYAN +
                         "[Anderson Acceleration]" +
                         PDCommon::Utils::Colors::RESET +
                         " Applied! Depth: " + std::to_string(mk));
              }
            }
          }

          NRIter++;
          isFirstExplicitTick_ = true; // 重置以驱动下一轮 ADR 内循环
          LOG_INFO("    " + PDCommon::Utils::Colors::MAGENTA + "[NR]" +
                   PDCommon::Utils::Colors::RESET + " iter " +
                   std::to_string(NRIter) + " / " +
                   std::to_string(maxNRIters_) + " | MacroForce: " +
                   PDCommon::Utils::StringUtils::toScientific(macroForceRatio) +
                   " (dFref=" +
                   PDCommon::Utils::StringUtils::toScientific(fIntIncrement) +
                   ")" + " | MacroDisp: " +
                   PDCommon::Utils::StringUtils::toScientific(macroDispRatio));
        }

      } // 结束 NR 迭代循环

      if (!NRConverged) {
        LOG_WARNING("    [Step " + std::to_string(stepConfig.stepId) +
                    " / Sub " + std::to_string(sub + 1) + "] " +
                    PDCommon::Utils::Colors::MAGENTA + "[NR]" +
                    PDCommon::Utils::Colors::RESET +
                    " did NOT converge within " + std::to_string(maxNRIters_) +
                    " iterations!");

        // =====================================================================
        // [Auto-Switch 降级回滚保护]
        // =====================================================================
        if (fractureStrategy_ == "StrictNRFracture" ||
            fractureStrategy_ == "LaggedNRFracture" ||
            fractureStrategy_ == "Staggered") {
          LOG_WARNING(
              "    >> " + PDCommon::Utils::Colors::RED + "[Auto-Switch]" +
              PDCommon::Utils::Colors::RESET +
              " Severe tearing detected! Rolling back substep and permanently "
              "switching to FastInnerLoop explicit dynamics...");

          for (size_t k = 0; k < soTargets_.size(); ++k) {
            const auto &so = soTargets_[k];
#pragma omp parallel for schedule(static)
            for (int i = 0; i < static_cast<int>(so.totalComponents); ++i) {
              so.uPtr[i] = dispBase_[k][i];
              so.vPtr[i] = 0.0;
              so.aPtr[i] = 0.0;
            }
          }
          isFirstExplicitTick_ = true;
          maxNRIters_ = 1;
          NRStateFrozen_ = false;
          fractureStrategy_ = "FastInnerLoop";
          sub--; // 重做当前子步
          continue;
        }
      }

      // [状态递进]
      // 保存本子步收敛时的总内力范数，供下一子步外循环增量基准使用
      fIntPrevSubstep_ = fIntTotal;
      ctx.getFieldManager().executeAllRegisteredSwaps();
      for (auto &[matName, matPtr] : ctx.getMaterialManager().getMaterials()) {
        if (matPtr)
          matPtr->commitState();
      }

      if (outputCallback) {
        globalSubstepCounter++;
        double prevPhysicalTime =
            (stepIdx == 0) ? 0.0 : loadStepConfigs_[stepIdx - 1].targetTime;
        double stepPhysicalTime = stepConfig.targetTime;
        double substepFraction =
            static_cast<double>(sub + 1) / currentNumSubsteps;
        double currentPhysicalTime =
            prevPhysicalTime +
            substepFraction * (stepPhysicalTime - prevPhysicalTime);
        outputCallback(globalSubstepCounter, currentPhysicalTime);
      }
    }

    // 宣告本 LoadStep 彻底结束，让 Boundary Conditions 记忆最终态
    BC::commitEndStep(ctx.getBCManager(), stepConfig.stepId);
  }

  LOG_INFO("--- ADR Complete | Total Substeps: " +
           std::to_string(globalSubstepCounter) + "  |  Pure Compute: " +
           PDCommon::Utils::StringUtils::toScientific(timer.pureComputeTime()) +
           "s" + "  |  Total: " +
           PDCommon::Utils::StringUtils::toScientific(timer.totalElapsed()) +
           "s" + "  |  Speed: " +
           std::to_string(static_cast<int>(timer.pureSpeed())) + " steps/s");

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
  dispNROld_.resize(soTargets_.size());
  forceCorr_.resize(soTargets_.size());

  for (size_t k = 0; k < soTargets_.size(); ++k) {
    velHalfOld_[k].assign(soTargets_[k].totalComponents, 0.0);
    aOld_[k].assign(soTargets_[k].totalComponents, 0.0);
    dispOld_[k].assign(soTargets_[k].totalComponents, 0.0);
    dispBase_[k].assign(soTargets_[k].totalComponents, 0.0);
    dispNROld_[k].assign(soTargets_[k].totalComponents, 0.0);
    forceCorr_[k].assign(soTargets_[k].totalComponents, 0.0);
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

void ADR_Integrator::saveNROldDisplacement() {
  for (size_t k = 0; k < soTargets_.size(); ++k) {
    const auto &so = soTargets_[k];
#pragma omp parallel for schedule(static)
    for (int i = 0; i < static_cast<int>(so.totalComponents); ++i) {
      dispNROld_[k][i] = so.uPtr[i];
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

        int particle_idx = i / so.dimension;
        double effective_mass = nodalMass_[particle_idx] * massScaleFactor_;

        // 瑞利商积分同样引入质量权重，使其成为真实的能量差商
        local_cn1 -= effective_mass * dU_step * da;
        local_cn2 += effective_mass * dU_step * dU_step;
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

void ADR_Integrator::computeConvergenceCriteria(double currentFRef, const double* damage) {
  kineticRatio_ = 0.0;
  dispRatio_ = 0.0;
  forceRatio_ = 0.0;
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
      int particle_idx = i / so.dimension;

      // [死点屏蔽]：若该点损伤过大，其位置波动和不平衡力对整体无影响，不纳入统计
      if (damage && damage[particle_idx] > 0.99) {
        continue;
      }

      double du_iter = so.uPtr[i] - dispOld_[k][i];
      double du_substep = so.uPtr[i] - dispBase_[k][i];

      double effective_mass = nodalMass_[particle_idx] * massScaleFactor_;
      double f_unbalanced = so.aPtr[i] * effective_mass;

      // 动能计算：真正的缩放质量焦耳能量 = 0.5 * m_eff * v^2
      local_tol1 +=
          0.5 * effective_mass * velHalfOld_[k][i] * velHalfOld_[k][i];
      local_tol2 += du_iter * du_iter;
      local_denom_tol2 += du_substep * du_substep;
      local_tol3 += f_unbalanced * f_unbalanced;
    }
    kineticRatio_ += local_tol1;
    dispRatio_ += local_tol2;
    denom_TOL2 += local_denom_tol2;
    forceRatio_ += local_tol3;
  }

  double currentKinetic = kineticRatio_; // 现在是真正的标量动能 (Joule)
  if (currentKinetic > maxKineticEnergy_) {
    maxKineticEnergy_ = currentKinetic;
  }
  // 如果 peak 极小，说明系统从一开始就没动过，避免除零
  double effectiveMaxK = std::max(maxKineticEnergy_, 1.0e-24);
  kineticRatio_ = currentKinetic / effectiveMaxK;

  dispRatio_ = std::sqrt(dispRatio_);
  denom_TOL2 = std::sqrt(denom_TOL2);
  forceRatio_ = std::sqrt(forceRatio_);

  // 使用相对位移进行 TOL2 判定（防阶跃/静止状态微扰除零）
  double effective_denom2 = std::max(denom_TOL2, 1.0e-8);
  dispRatio_ = dispRatio_ / effective_denom2;

  // [统一收敛架构]：直接使用实时的全场内力增量基准（currentFRef）计算真实的工程物理残差比
  forceRatio_ = forceRatio_ / currentFRef;
}

// ---------------------------------------------------------------------------
// computeNRConvergence：ANSYS 风格宏观收敛准则
// 力准则：||R_free|| / max(||F_int_all||, MINREF)  → 工程相对残差
// 位移准则：||du_NR|| / ||du_substep||            → 位移进展比
// ---------------------------------------------------------------------------
void ADR_Integrator::computeNRConvergence(double fIntTotal,
                                          double &macroForceRatio,
                                          double &macroDispRatio,
                                          const double* damage) {

  // --- 力残差分子：自由粒子的不平衡力 L2 范数 ---
  double sumResFreeSq = 0.0;
  for (size_t k = 0; k < soTargets_.size(); ++k) {
    const auto &so = soTargets_[k];
    double localSq = 0.0;
#pragma omp parallel for reduction(+ : localSq) schedule(static)
    for (int i = 0; i < static_cast<int>(so.totalComponents); ++i) {
      int particle_idx = i / so.dimension;

      // [死点屏蔽]：若该点损伤过大，其位置波动和不平衡力对整体无影响，不纳入统计
      if (damage && damage[particle_idx] > 0.99) {
        continue;
      }

      double effective_mass = nodalMass_[particle_idx] * massScaleFactor_;
      double f = so.aPtr[i] * effective_mass;
      localSq += f * f;
    }
    sumResFreeSq += localSq;
  }
  double resFree = std::sqrt(sumResFreeSq);

  // --- 力残差比 ---
  double fRef = std::max(fIntTotal, minRefForce_);
  macroForceRatio = resFree / fRef;

  // --- 位移残差比：本轮外循环位移增量 / 本子步总位移增量 ---
  double sumDuNRSq = 0.0;
  double sumDuSubstepSq = 0.0;
  for (size_t k = 0; k < soTargets_.size(); ++k) {
    const auto &so = soTargets_[k];
    double localNRSq = 0.0;
    double localSubstepSq = 0.0;
#pragma omp parallel for reduction(+ : localNRSq, localSubstepSq)              \
    schedule(static)
    for (int i = 0; i < static_cast<int>(so.totalComponents); ++i) {
      double du_NR = so.uPtr[i] - dispNROld_[k][i];
      double du_sub = so.uPtr[i] - dispBase_[k][i];
      localNRSq += du_NR * du_NR;
      localSubstepSq += du_sub * du_sub;
    }
    sumDuNRSq += localNRSq;
    sumDuSubstepSq += localSubstepSq;
  }
  double denomDisp = std::max(std::sqrt(sumDuSubstepSq), 1.0e-30);
  macroDispRatio = std::sqrt(sumDuNRSq) / denomDisp;
}

void ADR_Integrator::initializeNodalMass(PDCommon::Core::PDContext &ctx) {
  auto &pm = ctx.getParticleManager();
  const auto &particles = pm.getAllParticles();
  size_t numParticles = particles.size();
  nodalMass_.assign(numParticles, 1.0);

  auto *volumeField = ctx.getFieldManager().getFieldAs<double>("Volume");
  const double *volumes = volumeField ? volumeField->dataPtr() : nullptr;

  for (size_t i = 0; i < numParticles; ++i) {
    double vol = volumes ? volumes[i] : 1.0;
    double rho = 1.0;
    auto *mat = dynamic_cast<PDCommon::Material::MechanicalMaterial *>(
        particles[i].getMaterial());
    if (mat) {
      rho = mat->getDensity();
    }
    nodalMass_[i] = rho * vol;
  }
}

} // namespace Src::Integration

REGISTER_TIME_INTEGRATOR(ADR, Src::Integration::ADR_Integrator)
