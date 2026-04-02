// ============================================================================
// TimeIntegrator.cpp - 时间推进基类公共实现
// ============================================================================

#include "TimeIntegrator.h"
#include "BCManager.h"
#include "FieldManager.h"
#include "Logger.h"
#include "MaterialManager.h"
#include "MechanicalMaterial.h"
#include "PDKernel.h"
#include "TypedField.h"
#include <cmath>

namespace Src::Integration {

void TimeIntegrator::configure(const YAML::Node &solverNode) {
    if (solverNode["TimeStep_dt"]) {
      dt_ = solverNode["TimeStep_dt"].as<double>();
      autoCalcDt_ = false; // 用户显式指定了 dt，关闭自动计算
    }
    if (solverNode["TotalTime"])
      totalTime_ = solverNode["TotalTime"].as<double>();
    if (solverNode["OutputInterval"])
      outputInterval_ = solverNode["OutputInterval"].as<int>();
}

void TimeIntegrator::evaluateForces(
    PDCommon::Core::PDContext &ctx,
    std::vector<std::unique_ptr<PDKernel>> &kernels,
    const std::vector<std::string> &rateFieldsToClear) {

  auto &fieldManager = ctx.getFieldManager();
  auto &bcManager = ctx.getBCManager();

  // 1. 清零指定的率场
  for (const auto &name : rateFieldsToClear) {
    auto *field = fieldManager.getFieldAs<double>(name);
    if (field) field->clearToZero();
  }

  // 2. 施加 Neumann 源项
  bcManager.applySources();

  // 3. 计算内力
  for (auto &kernel : kernels) {
    kernel->computeForceState(ctx);
  }
}

// ---------------------------------------------------------------------------
// computeCFLTimestep：基于 CFL 条件自动计算时间步长（通用方法）
// dt = safetyFactor * dx / sqrt(E / (rho * massScale))
// ---------------------------------------------------------------------------
void TimeIntegrator::computeCFLTimestep(PDCommon::Core::PDContext &ctx,
                                        double massScale,
                                        double safetyFactor) {
  auto &matManager = ctx.getMaterialManager();
  auto &fieldManager = ctx.getFieldManager();

  // 通过粒子体积估算间距 dx = V^(1/3)
  auto *volumeField = fieldManager.getFieldAs<double>("Volume");
  if (!volumeField) {
    LOG_WARNING("[TimeIntegrator] Volume field not found, cannot auto-calc dt. "
                "Using dt = " + std::to_string(dt_));
    return;
  }

  const double *volumes = volumeField->dataPtr();
  double dx = std::cbrt(volumes[0]);

  // 遍历所有材料，取最大有效波速（最保守的 dt）
  double maxWaveSpeed = 0.0;
  for (const auto &[name, matPtr] : matManager.getMaterials()) {
    auto *mechMat = dynamic_cast<PDCommon::Material::MechanicalMaterial *>(
        matPtr.get());
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
    dt_ = safetyFactor * dx / maxWaveSpeed;
    LOG_INFO("[" + getName() + "] Auto CFL dt: dx = " + std::to_string(dx) +
             ", c = " + std::to_string(maxWaveSpeed) +
             ", massScale = " + std::to_string(massScale) +
             ", dt = " + std::to_string(dt_));
  } else {
    LOG_WARNING("[" + getName() + "] No valid MechanicalMaterial for CFL. "
                "Using default dt = " + std::to_string(dt_));
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
            out_soTargets.push_back({
                tb.primaryField, ta.primaryField, ta.rateField,
                uField->dataPtr(), vField->dataPtr(), aField->dataPtr(),
                numParticles * tb.dimension,
                tb.dimension
            });
            out_accFieldNames.push_back(ta.rateField);
            LOG_INFO("[" + getName() + "] Identified 2nd-order ODE coupling: " +
                     tb.primaryField + " <- " + ta.primaryField + " <- " + ta.rateField);
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
        LOG_WARNING("[" + getName() + "] Field '" + target.primaryField + "' or '" +
                    target.rateField + "' not found, skipping.");
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
        out_foTargets.push_back({
            target.primaryField, target.rateField,
            primaryField->dataPtr(), rateField->dataPtr(),
            numParticles * static_cast<size_t>(target.dimension),
            target.dimension
        });
        out_rateFieldNames.push_back(target.rateField);
        LOG_INFO("[" + getName() + "] Identified 1st-order ODE integration target: " +
                 target.primaryField + " <- " + target.rateField);
      }
    }
  }
}

} // namespace Src::Integration
