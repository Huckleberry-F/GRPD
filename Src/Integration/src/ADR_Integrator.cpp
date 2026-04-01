// ============================================================================
// ADR_Integrator.cpp - 自适应动态松弛 / 显式准静态积分器 (Leapfrog/Verlet 格式)
// ============================================================================

#include "ADR_Integrator.h"
#include "BCManager.h"
#include "FieldManager.h"
#include "Logger.h"
#include "Material.h"
#include "MaterialManager.h"
#include "PDKernel.h"
#include "ParticleManager.h"
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
  if (solverNode["LogInterval"])
    logInterval_ = solverNode["LogInterval"].as<int>();
}

void ADR_Integrator::run(PDCommon::Core::PDContext &ctx,
                         std::vector<std::unique_ptr<PDKernel>> &kernels,
                         std::function<void(int, double)> outputCallback) {
  const double dt = dt_;

  auto &fieldManager = ctx.getFieldManager();
  auto &bcManager = ctx.getBCManager();
  const size_t numParticles = ctx.getParticleManager().getTotalParticles();

  std::vector<PDKernel::IntegrationTarget> targets;
  for (auto &kernel : kernels) {
    auto t = kernel->getIntegrationTargets();
    targets.insert(targets.end(), t.begin(), t.end());
  }

  // 寻找纯力学二阶积分目标
  std::vector<SecondOrderTarget> soTargets;
  std::vector<std::string> accFieldNames;

  for (const auto &tb : targets) {
    for (const auto &ta : targets) {
      if (tb.rateField == ta.primaryField && tb.dimension == ta.dimension) {
        auto *uField = fieldManager.getFieldAs<double>(tb.primaryField);
        auto *vField = fieldManager.getFieldAs<double>(ta.primaryField);
        auto *aField = fieldManager.getFieldAs<double>(ta.rateField);

        if (uField && vField && aField) {
          bool alreadyAdded = false;
          for (const auto &existing : soTargets) {
            if (existing.uPtr == uField->dataPtr()) {
              alreadyAdded = true;
              break;
            }
          }
          if (!alreadyAdded) {
            soTargets.push_back({tb.primaryField, ta.primaryField, ta.rateField,
                                 uField->dataPtr(), vField->dataPtr(),
                                 aField->dataPtr(), numParticles * tb.dimension,
                                 tb.dimension});
            accFieldNames.push_back(ta.rateField);
            LOG_INFO(
                "[ADR_Integrator] Found 2nd-order pair: " + tb.primaryField +
                " <- " + ta.primaryField + " <- " + ta.rateField);
          }
        }
      }
    }
  }

  if (soTargets.empty()) {
    LOG_ERROR("[ADR_Integrator] No 2nd-order ODE pairs found! Cannot run ADR.");
    return;
  }

  for (auto &kernel : kernels) {
    kernel->preCompute(ctx);
  }

  LOG_INFO("[ADR_Integrator] Starting Custom ADR: NumLoadSteps = " +
           std::to_string(numLoadSteps_) + ", NumSubsteps = " +
           std::to_string(numSubsteps_) + ", KBC = " + std::to_string(kbc_));

  // ADR Leapfrog 算法专属的历史状态向量。
  // 每个 soTarget 按自身实际的 totalComponents
  // 独立分配，避免用最大值齐平导致的内存浪费。
  std::vector<std::vector<double>> velHalfOld(soTargets.size());
  std::vector<std::vector<double>> aOld(soTargets.size());
  std::vector<std::vector<double>> dispOld(soTargets.size());
  std::vector<std::vector<double>> dispBase(soTargets.size()); // 基准位移

  for (size_t k = 0; k < soTargets.size(); ++k) {
    velHalfOld[k].assign(soTargets[k].totalComponents, 0.0);
    aOld[k].assign(soTargets[k].totalComponents, 0.0);
    dispOld[k].assign(soTargets[k].totalComponents, 0.0);
    dispBase[k].assign(soTargets[k].totalComponents, 0.0);
  }

  // 追踪整个显式积分会话的首次迭代，
  // 保持跨子步 (substeps) 时的动量连续性。
  bool isFirstExplicitTick = true;

  for (int step = 0; step < numLoadSteps_; ++step) {
    LOG_INFO("=== Load Step " + std::to_string(step) + " / " +
             std::to_string(numLoadSteps_ - 1) + " ===");

    for (int sub = 1; sub <= numSubsteps_; ++sub) {
      // 记录 Substep 开始的基准位移
      for (size_t k = 0; k < soTargets.size(); ++k) {
        const auto &so = soTargets[k];
#pragma omp parallel for schedule(static)
        for (int i = 0; i < static_cast<int>(so.totalComponents); ++i) {
          dispBase[k][i] = so.uPtr[i];
        }
      }

      // [1] 基于 ANSYS KBC 设置计算当前目标的载荷因子 (Load Factor)
      // step 从 0 开始，第 step 步的目标载荷因子 = (step+1)/numLoadSteps_，
      // 前一步的载荷因子 = step/numLoadSteps_。
      double targetLF = static_cast<double>(step + 1) / numLoadSteps_;
      double prevLF = static_cast<double>(step) / numLoadSteps_;
      double loadFactor = prevLF;

      if (kbc_ == 0) { // KBC, 0 : RAMP (Linear interpolation)
        loadFactor +=
            (targetLF - prevLF) * (static_cast<double>(sub) / numSubsteps_);
      } else { // KBC, 1 : STEP (Step up immediately)
        loadFactor = targetLF;
      }

      bool converged = false;
      int iter = 0;
      double TOL2_final = 0.0;

      while (!converged && iter < maxPseudoSteps_) {

        // 备份旧位移以用于 TOL2 收敛判定追踪
        for (size_t k = 0; k < soTargets.size(); ++k) {
          const auto &so = soTargets[k];
#pragma omp parallel for schedule(static)
          for (int i = 0; i < static_cast<int>(so.totalComponents); ++i) {
            dispOld[k][i] = so.uPtr[i];
          }
        }

        // [2] 节点内力与加速度计算 (P_new / m)
        // 内核将计算内力 F_int 并安全地自动转化为比加速度 a_new = F_int / m
        bcManager.applyConstraints(loadFactor);
        evaluateForces(ctx, kernels, accFieldNames);

        // [3] 计算自适应阻尼系数 (cn)
        // cn1 = cn1 - disp^2 * (a_new - a_old) / (dt * velhalfold)
        // cn2 = cn2 + disp^2
        double cn = 0.0;
        double cn1 = 0.0;
        double cn2 = 0.0;

        if (!isFirstExplicitTick) {
          for (size_t k = 0; k < soTargets.size(); ++k) {
            const auto &so = soTargets[k];
            double local_cn1 = 0.0;
            double local_cn2 = 0.0;

#pragma omp parallel for reduction(- : local_cn1) reduction(+ : local_cn2)     \
    schedule(static)
            for (int i = 0; i < static_cast<int>(so.totalComponents); ++i) {
              double dU = so.uPtr[i] - dispBase[k][i];
              double inv_vhalf = (std::abs(velHalfOld[k][i]) > 1e-16)
                                     ? (1.0 / velHalfOld[k][i])
                                     : 0.0;
              local_cn1 -= dU * dU * (so.aPtr[i] - aOld[k][i]) * inv_vhalf / dt;
              local_cn2 += dU * dU;
            }
            cn1 += local_cn1;
            cn2 += local_cn2;
          }
        }

        if (cn2 > 1e-30 && (cn1 / cn2) > 0.0) {
          cn = 2.0 * std::sqrt(cn1 / cn2);
        }
        if (cn > 1.8 / dt) {
          cn = 1.8 / dt; // equivalent to user's 0.9 * 2.0 / dt
        }

        // [4] ADR Leapfrog 积分公式更新
        for (size_t k = 0; k < soTargets.size(); ++k) {
          const auto &so = soTargets[k];
#pragma omp parallel for schedule(static)
          for (int i = 0; i < static_cast<int>(so.totalComponents); ++i) {
            double velHalfNew = 0.0;

            if (isFirstExplicitTick) { // 全局首个明确的积分步长 (冷启动)
              velHalfNew = 0.5 * dt * so.aPtr[i];
            } else {
              velHalfNew =
                  ((2.0 - cn * dt) * velHalfOld[k][i] + 2.0 * dt * so.aPtr[i]) /
                  (2.0 + cn * dt);
            }

            so.vPtr[i] = 0.5 * (velHalfOld[k][i] + velHalfNew);
            so.uPtr[i] = so.uPtr[i] + velHalfNew * dt;

            velHalfOld[k][i] = velHalfNew;
            aOld[k][i] = so.aPtr[i];
          }
        }

        // [5] 重新施加约束以严格冻结位移边界条件
        bcManager.applyConstraints(loadFactor);

        // 允许各种模型计算损伤或内部状态演化
        for (auto &kernel : kernels) {
          kernel->postCompute(ctx);
        }

        // [6] 收敛检测: 基于位移增量(TOL2)和残余力(TOL3)
        double TOL2 = 0.0;
        double TOL3 = 0.0;
        for (size_t k = 0; k < soTargets.size(); ++k) {
          const auto &so = soTargets[k];
          double local_tol2 = 0.0;
          double local_tol3 = 0.0;

#pragma omp parallel for reduction(+ : local_tol2, local_tol3) schedule(static)
          for (int i = 0; i < static_cast<int>(so.totalComponents); ++i) {
            double du = so.uPtr[i] - dispOld[k][i];
            local_tol2 += du * du;
            local_tol3 += so.aPtr[i] * so.aPtr[i];
            // 这里 aPtr^2 与 (残余力/质量)^2 高度相关，用作收敛判定标准。
          }
          TOL2 += local_tol2;
          TOL3 += local_tol3;
        }

        TOL2 = std::sqrt(TOL2);
        TOL3 = std::sqrt(TOL3);
        TOL2_final = TOL2;

        // 如果位移增量极小 且 加速度残差(不平衡力)极低，则判定当前时间步已收敛
        if (TOL2 < dispTol_ && TOL3 < forceTol_ && iter > 20) {
          converged = true;
          LOG_INFO("    [Step " + std::to_string(step) + " / Sub " +
                   std::to_string(sub) +
                   "] Converged. Iter: " + std::to_string(iter) +
                   " | TOL2(dU): " + std::to_string(TOL2) + " | TOL3(Res): " +
                   std::to_string(TOL3) + " | cn: " + std::to_string(cn));
        }

        if (iter % logInterval_ == 0 && iter > 0) {
          LOG_INFO("    > Step " + std::to_string(step) + " / Sub " +
                   std::to_string(sub) + " Iter " + std::to_string(iter) +
                   " | LF: " + std::to_string(loadFactor) + " | TOL2: " +
                   std::to_string(TOL2) + " | cn: " + std::to_string(cn));
        }

        iter++;
        isFirstExplicitTick = false;
      }

      if (!converged) {
        LOG_WARNING("    [Step " + std::to_string(step) + " / Sub " +
                    std::to_string(sub) +
                    "] Reached MaxPseudoSteps without convergence! TOL2 = " +
                    std::to_string(TOL2_final));
        // TODO: Implement auto-bisection of load step here (自适应降载)
      }

      // [关键部位] 当前 Substep 达到了稳定平衡态，固化不可逆的状态转换 (例如塑性应变、损伤演化)
      auto &matMap = ctx.getMaterialManager().getMaterials();
      for (auto &[matName, matPtr] : matMap) {
        if (matPtr)
          matPtr->commitState();
      }

      // 通常在每一个收敛的 Substep 后输出文件，使用伪时间或载荷因子作为时间轴表现
      if (outputCallback) {
        // step 从 0 开始，全局输出帧号 = step * numSubsteps_ + sub（从 1
        // 起单调递增）
        outputCallback(step * numSubsteps_ + sub, loadFactor);
      }
    }
  }
}

} // namespace Src::Integration

REGISTER_TIME_INTEGRATOR(ADR, Src::Integration::ADR_Integrator)
