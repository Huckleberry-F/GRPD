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

void TemperatureBC::apply(double loadFactor) {
  if (!temperature_)
    return;
  double activeVal = prevVal_ + (temperatureVal_ - prevVal_) * loadFactor;
  temperature_->set(particleId_, activeVal);
}

void TemperatureBC::commitEndStep() {
  if (!temperature_ || particleId_ < 0)
    return;
  prevVal_ = temperature_->get(particleId_);
}

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
  vol_ = volField ? volField->get(particleId) : 1.0;

  baseFlux_ = values[0];
}

void HeatFluxBC::setScalingFactors(double dx, double density, double massScale, int dim, double thickness) {
  double local_dx = (dim == 2) ? std::sqrt(vol_ / thickness) : std::cbrt(vol_);
  flux_ = baseFlux_ / local_dx;
}

void HeatFluxBC::apply() {
  tempRate_->add(particleId_, flux_);
  heatFlux_->set(particleId_, flux_);
}

void HeatFluxBC::apply(double loadFactor) {
  if (!tempRate_ || !heatFlux_)
    return;
  double activeVal = prevVal_ + (flux_ - prevVal_) * loadFactor;
  tempRate_->add(particleId_, activeVal);
  heatFlux_->set(particleId_, activeVal);
}

void HeatFluxBC::commitEndStep() {
  if (particleId_ < 0)
    return;
  prevVal_ = flux_; // 源项认为已经插值到达理想终态
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
  vol_ = volField ? volField->get(particleId) : 1.0;

  baseHConv_ = values[0];
  tInf_ = (values.size() > 1) ? values[1] : 0.0;
}

void ConvectionBC::setScalingFactors(double dx, double density, double massScale, int dim, double thickness) {
  double local_dx = (dim == 2) ? std::sqrt(vol_ / thickness) : std::cbrt(vol_);
  hConv_ = baseHConv_ / local_dx;
}

void ConvectionBC::apply() {
  double currentT = temperature_->get(particleId_);
  double qConv = hConv_ * (tInf_ - currentT);
  tempRate_->add(particleId_, qConv);
  heatFlux_->set(particleId_, qConv);
}

void ConvectionBC::apply(double loadFactor) {
  if (!temperature_ || !tempRate_ || !heatFlux_)
    return;
  // 对流中的 T_inf 根据 loadFactor 进行插值
  double activeTInf = prevTInf_ + (tInf_ - prevTInf_) * loadFactor;
  double currentT = temperature_->get(particleId_);
  double qConv = hConv_ * (activeTInf - currentT);
  tempRate_->add(particleId_, qConv);
  heatFlux_->set(particleId_, qConv);
}

void ConvectionBC::commitEndStep() {
  if (particleId_ < 0)
    return;
  prevTInf_ = tInf_; // 对流温度认为已经插值到达理想终态
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
