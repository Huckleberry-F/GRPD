// ============================================================================
// ThermalBC.cpp - 热边界条件派生类实现 + 工厂注册
// ============================================================================

#include "ThermalBC.h"
#include "BCRegistry.h"
#include "FieldManager.h"
#include <cmath>

namespace PDCommon::BC {

// ===========================================================================
// TemperatureBC (Dirichlet)
// ===========================================================================
void TemperatureBC::initialize(PDCommon::Field::FieldManager &fieldManager,
                               int particleId,
                               const std::vector<double> &values) {
  // 从 FieldManager 获取各物理场指针
  temperature_ = fieldManager.getFieldAs<double>("Temperature");
  tempRate_ = fieldManager.getFieldAs<double>("TempRate");
  heatFlux_ = fieldManager.getFieldAs<double>("HeatFlux");
  particleId_ = particleId;
  temperatureVal_ = values[0];
}

void TemperatureBC::apply() { temperature_->set(particleId_, temperatureVal_); }

// ===========================================================================
// HeatFluxBC (Neumann)
// ===========================================================================
void HeatFluxBC::initialize(PDCommon::Field::FieldManager &fieldManager,
                            int particleId, const std::vector<double> &values) {
  temperature_ = fieldManager.getFieldAs<double>("Temperature");
  tempRate_ = fieldManager.getFieldAs<double>("TempRate");
  heatFlux_ = fieldManager.getFieldAs<double>("HeatFlux");
  particleId_ = particleId;

  auto *volField = fieldManager.getFieldAs<double>("Volume");
  double vol = volField ? volField->get(particleId) : 1.0;
  double dx = std::cbrt(vol);

  // 等效体积热源换算: Q_v = q_s / dx
  flux_ = values[0] / dx;
}

void HeatFluxBC::apply() {
  tempRate_->add(particleId_, flux_);
  heatFlux_->set(particleId_, flux_);
}

// ===========================================================================
// ConvectionBC (Robin)
// ===========================================================================
void ConvectionBC::initialize(PDCommon::Field::FieldManager &fieldManager,
                              int particleId,
                              const std::vector<double> &values) {
  temperature_ = fieldManager.getFieldAs<double>("Temperature");
  tempRate_ = fieldManager.getFieldAs<double>("TempRate");
  heatFlux_ = fieldManager.getFieldAs<double>("HeatFlux");
  particleId_ = particleId;

  auto *volField = fieldManager.getFieldAs<double>("Volume");
  double vol = volField ? volField->get(particleId) : 1.0;
  double dx = std::cbrt(vol);

  // 等效体积对流系数换算
  hConv_ = values[0] / dx;
  tInf_ = (values.size() > 1) ? values[1] : 0.0;
}

void ConvectionBC::apply() {
  double currentT = temperature_->get(particleId_);
  double qConv = hConv_ * (tInf_ - currentT);
  tempRate_->add(particleId_, qConv);
  heatFlux_->set(particleId_, qConv);
}

// ============================================================================
// 编译期工厂注册：将具体子类以 .grpd 中的 Type 名注册到 BCRegistry
// ============================================================================
REGISTER_BC_TYPE(T, [](const std::string &name) {
  return std::make_unique<TemperatureBC>(name);
});

REGISTER_BC_TYPE(FLUX, [](const std::string &name) {
  return std::make_unique<HeatFluxBC>(name);
});

REGISTER_BC_TYPE(CONV, [](const std::string &name) {
  return std::make_unique<ConvectionBC>(name);
});

} // namespace PDCommon::BC
