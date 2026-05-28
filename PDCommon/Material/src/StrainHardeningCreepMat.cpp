#include "StrainHardeningCreepMat.h"
#include "MaterialRegistry.h"
#include <cmath>
#include <algorithm>

namespace PDCommon::Material {

REGISTER_MATERIAL_TYPE(StrainHardeningCreepMat, [](const std::string &name) {
  return std::make_unique<PDCommon::Material::StrainHardeningCreepMat>(name);
});

StrainHardeningCreepMat::StrainHardeningCreepMat(const std::string &name) : CreepMaterialBase(name) {}

void StrainHardeningCreepMat::initialize(const YAML::Node &matNode) {
  CreepMaterialBase::initialize(matNode);
  if (matNode["m"]) {
    m_ = matNode["m"].as<double>();
  }
}

double StrainHardeningCreepMat::computeDeltaCreepStrain(double q_trial, double dt, double t, double eq_cr_old) const {
  // Strain Hardening: d(eps_cr) / dt = A * q^n * (eps_cr)^m
  // Avoid singularity if eps_cr=0
  double eff_eps = std::max(eq_cr_old, 1e-8);
  double rate = A_ * std::pow(q_trial, n_) * std::pow(eff_eps, m_);
  return rate * dt;
}

} // namespace PDCommon::Material
