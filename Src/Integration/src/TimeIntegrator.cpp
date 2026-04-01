// ============================================================================
// TimeIntegrator.cpp - 时间推进基类公共实现
// ============================================================================

#include "TimeIntegrator.h"
#include "BCManager.h"
#include "FieldManager.h"
#include "PDKernel.h"
#include "TypedField.h"

namespace Src::Integration {

void TimeIntegrator::configure(const YAML::Node &solverNode) {
    if (solverNode["TimeStep_dt"])
      dt_ = solverNode["TimeStep_dt"].as<double>();
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

} // namespace Src::Integration
