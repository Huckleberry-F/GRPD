#include "TimeHardeningCreepMat.h"
#include "MaterialRegistry.h"
#include <cmath>
#include <algorithm>

namespace PDCommon::Material {

REGISTER_MATERIAL_TYPE(TimeHardeningCreepMat, [](const std::string &name) {
  return std::make_unique<PDCommon::Material::TimeHardeningCreepMat>(name);
});

TimeHardeningCreepMat::TimeHardeningCreepMat(const std::string &name) : CreepMaterialBase(name) {}

void TimeHardeningCreepMat::initialize(const YAML::Node &matNode) {
  CreepMaterialBase::initialize(matNode);
  if (matNode["m"]) {
    m_ = matNode["m"].as<double>();
  }
}

double TimeHardeningCreepMat::computeDeltaCreepStrain(double q_trial, double dt, double t, double eq_cr_old) const {
  // Time Hardening: d(eps_cr) / dt = A * q^n * t^m
  // Avoid singularity if t=0 and m < 0
  double eff_t = std::max(t, 1e-8);
  double rate = A_ * std::pow(q_trial, n_) * std::pow(eff_t, m_);
  return rate * dt;
}

} // namespace PDCommon::Material
