// ============================================================================
// TimeIntegrator.cpp - 时间推进基类公共实现
// ============================================================================

#include "TimeIntegrator.h"
#include "BCManager.h"
#include "ContactManager.h"
#include "FieldManager.h"
#include "Logger.h"
#include "StringUtils.h"
#include "MaterialManager.h"
#include "MechanicalMaterial.h"
#include "PDKernel.h"
#include "TypedField.h"
#include <cmath>
#include <omp.h>


namespace Src::Integration {

using PDCommon::BC::BC;

void TimeIntegrator::configure(const YAML::Node &solverNode) {
  if (solverNode["TimeStep_dt"]) {
    dt_ = solverNode["TimeStep_dt"].as<double>();
    autoCalcDt_ = false; // 用户显式指定了 dt，关闭自动计算
  }
  if (solverNode["TotalTime"]) {
    totalTime_ = solverNode["TotalTime"].as<double>();
    defaultEndTime_ = totalTime_;
  }
  if (solverNode["KBC"]) {
    kbc_ = solverNode["KBC"].as<int>();
  }
  if (solverNode["NumLoadSteps"])
    defaultNumLoadSteps_ = solverNode["NumLoadSteps"].as<int>();
  if (solverNode["NumSubsteps"])
    defaultNumSubsteps_ = solverNode["NumSubsteps"].as<int>();

  if (solverNode["OutputInterval"])
    outputInterval_ = solverNode["OutputInterval"].as<int>();

  if (solverNode["OMP_Threads"]) {
    int threads = solverNode["OMP_Threads"].as<int>();
    omp_set_num_threads(threads);
    LOG_INFO("[TimeIntegrator] Explicitly set OMP Threads to: " +
             std::to_string(threads));
  }

  // 统一解析 LoadSteps 序列
  loadStepConfigs_.clear();
  if (solverNode["LoadSteps"] && solverNode["LoadSteps"].IsSequence()) {
    for (size_t i = 0; i < solverNode["LoadSteps"].size(); ++i) {
      auto stepNode = solverNode["LoadSteps"][i];
      LoadStepConfig config;
      config.stepId = static_cast<int>(i) + 1;
      if (stepNode["Step"])
        config.stepId = stepNode["Step"].as<int>();

      config.numSubsteps = stepNode["NumSubsteps"]
                               ? stepNode["NumSubsteps"].as<int>()
                               : defaultNumSubsteps_;

      if (stepNode["Time"]) {
        config.targetTime = stepNode["Time"].as<double>();
      } else if (stepNode["TargetTime"]) {
        config.targetTime = stepNode["TargetTime"].as<double>();
      } else {
        config.targetTime = defaultEndTime_;
      }

      if (stepNode["TimeStep_dt"])
        config.userDt = stepNode["TimeStep_dt"].as<double>();
      if (stepNode["KBC"])
        config.kbc = stepNode["KBC"].as<int>();

      loadStepConfigs_.push_back(config);
    }
  } else {
    // 没有任何显式定义序列时，退化为默认的一组均等划分或单一物理时段
    int steps = std::max(1, defaultNumLoadSteps_);
    for (int i = 0; i < steps; ++i) {
      LoadStepConfig config;
      config.stepId = i + 1;
      config.numSubsteps = defaultNumSubsteps_;
      // 显式总时间按步数均分（如果缺省按步分段计算）
      config.targetTime = defaultEndTime_ * static_cast<double>(i + 1) /
                          static_cast<double>(steps);
      loadStepConfigs_.push_back(config);
    }
  }
}

void TimeIntegrator::evaluateForces(
    PDCommon::Core::PDContext &ctx,
    std::vector<std::unique_ptr<PDKernel>> &kernels,
    const std::vector<std::string> &rateFieldsToClear, int currentStep,
    double activeLF) {

  auto &fieldManager = ctx.getFieldManager();

  // 1. 清零指定的率场
  for (const auto &name : rateFieldsToClear) {
    auto *field = fieldManager.getFieldAs<double>(name);
    if (field)
      field->clearToZero();
  }

  // 2. 施加 Neumann 源项
  BC::applySources(ctx.getBCManager(), activeLF, currentStep);

  // 3. 接触系统源项注入（由调用方直接遍历接触对，Manager 不参与调度）
  for (auto &[name, contact] : ctx.getContactManager().getContactPairs()) {
    contact->computeContactForce(ctx);
  }

  // 4. 计算内力
  for (auto &kernel : kernels) {
    kernel->computeForceState(ctx);
  }
}

// ---------------------------------------------------------------------------
// computeCFLTimestep：基于 CFL 条件自动计算时间步长（通用方法）
// dt = safetyFactor * dx / sqrt(E / (rho * massScale))
// ---------------------------------------------------------------------------
double TimeIntegrator::computeCFLTimestep(PDCommon::Core::PDContext &ctx,
                                          double massScale, double safetyFactor) {
  auto &matManager = ctx.getMaterialManager();
  auto &fieldManager = ctx.getFieldManager();

  // 通过粒子体积估算间距 dx = V^(1/3)
  auto *volumeField = fieldManager.getFieldAs<double>("Volume");
  if (!volumeField) {
    LOG_WARNING("[TimeIntegrator] Volume field not found, cannot auto-calc dt. "
                "Returning current dt.");
    return dt_;
  }

  const double *volumes = volumeField->dataPtr();
  double dx = std::cbrt(volumes[0]);

  // 遍历所有材料，取最大有效波速（最保守的 dt）
  double maxWaveSpeed = 0.0;
  for (const auto &[name, matPtr] : matManager.getMaterials()) {
    auto *mechMat =
        dynamic_cast<PDCommon::Material::MechanicalMaterial *>(matPtr.get());
    if (mechMat) {
      double E = mechMat->getYoungsModulus();
      double rho = mechMat->getDensity();
      if (rho > 1e-30 && E > 0.0) {
        double rho_eff = rho * massScale;
        double c_eff = std::sqrt(E / rho_eff);
        if (c_eff > maxWaveSpeed) {
          maxWaveSpeed = c_eff;
        }
      }
    }
  }

  if (maxWaveSpeed > 1e-30) {
    double limitDt = safetyFactor * dx / maxWaveSpeed;
    LOG_INFO("[" + getName() + "] Computed safe CFL limit dt: " + PDCommon::Utils::StringUtils::toScientific(limitDt) +
             " (dx = " + PDCommon::Utils::StringUtils::toScientific(dx) + ", c = " + PDCommon::Utils::StringUtils::toScientific(maxWaveSpeed) + ")");
    return limitDt;
  } else {
    LOG_WARNING("[" + getName() +
                "] No valid MechanicalMaterial for CFL. Returning current dt.");
    return dt_;
  }
}

void TimeIntegrator::extractSecondOrderTargets(
    const std::vector<std::unique_ptr<PDKernel>> &kernels,
    PDCommon::Core::PDContext &ctx,
    std::vector<SecondOrderTarget> &out_soTargets,
    std::vector<std::string> &out_accFieldNames) {

  out_soTargets.clear();
  out_accFieldNames.clear();

  std::vector<PDKernel::IntegrationTarget> kernelsTargets;
  for (const auto &kernel : kernels) {
    auto t = kernel->getIntegrationTargets();
    kernelsTargets.insert(kernelsTargets.end(), t.begin(), t.end());
  }

  auto &fieldManager = ctx.getFieldManager();
  const size_t numParticles = ctx.getParticleManager().getTotalParticles();

  // 泛型拓扑检测：双层循环寻找 target_b 的 rate 碰巧是 target_a 的 primary
  for (const auto &tb : kernelsTargets) {
    for (const auto &ta : kernelsTargets) {
      if (tb.rateField == ta.primaryField && tb.dimension == ta.dimension) {
        auto *uField = fieldManager.getFieldAs<double>(tb.primaryField);
        auto *vField = fieldManager.getFieldAs<double>(ta.primaryField);
        auto *aField = fieldManager.getFieldAs<double>(ta.rateField);

        if (uField && vField && aField) {
          bool alreadyAdded = false;
          for (const auto &existing : out_soTargets) {
            if (existing.uPtr == uField->dataPtr()) {
              alreadyAdded = true;
              break;
            }
          }
          if (!alreadyAdded) {
            out_soTargets.push_back(
                {tb.primaryField, ta.primaryField, ta.rateField,
                 uField->dataPtr(), vField->dataPtr(), aField->dataPtr(),
                 numParticles * tb.dimension, tb.dimension});
            out_accFieldNames.push_back(ta.rateField);
            LOG_INFO("[" + getName() +
                     "] Identified 2nd-order ODE coupling: " + tb.primaryField +
                     " <- " + ta.primaryField + " <- " + ta.rateField);
          }
        }
      }
    }
  }
}

void TimeIntegrator::extractFirstOrderTargets(
    const std::vector<std::unique_ptr<PDKernel>> &kernels,
    PDCommon::Core::PDContext &ctx,
    std::vector<FirstOrderTarget> &out_foTargets,
    std::vector<std::string> &out_rateFieldNames) {

  out_foTargets.clear();
  out_rateFieldNames.clear();

  auto &fieldManager = ctx.getFieldManager();
  const size_t numParticles = ctx.getParticleManager().getTotalParticles();

  for (auto &kernel : kernels) {
    auto targets = kernel->getIntegrationTargets();
    for (const auto &target : targets) {
      auto *primaryField = fieldManager.getFieldAs<double>(target.primaryField);
      auto *rateField = fieldManager.getFieldAs<double>(target.rateField);

      if (!primaryField || !rateField) {
        LOG_WARNING("[" + getName() + "] Field '" + target.primaryField +
                    "' or '" + target.rateField + "' not found, skipping.");
        continue;
      }

      bool alreadyAdded = false;
      for (const auto &existing : out_foTargets) {
        if (existing.primaryPtr == primaryField->dataPtr()) {
          alreadyAdded = true;
          break;
        }
      }

      if (!alreadyAdded) {
        out_foTargets.push_back(
            {target.primaryField, target.rateField, primaryField->dataPtr(),
             rateField->dataPtr(),
             numParticles * static_cast<size_t>(target.dimension),
             target.dimension});
        out_rateFieldNames.push_back(target.rateField);
        LOG_INFO("[" + getName() +
                 "] Identified 1st-order ODE integration target: " +
                 target.primaryField + " <- " + target.rateField);
      }
    }
  }
}

} // namespace Src::Integration
