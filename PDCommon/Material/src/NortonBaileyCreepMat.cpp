#include "NortonBaileyCreepMat.h"
#include "MaterialRegistry.h"
#include <cmath>

namespace PDCommon::Material {

REGISTER_MATERIAL_TYPE(NortonBaileyCreepMat, [](const std::string &name) {
  return std::make_unique<PDCommon::Material::NortonBaileyCreepMat>(name);
});

NortonBaileyCreepMat::NortonBaileyCreepMat(const std::string &name) : CreepMaterialBase(name) {}

double NortonBaileyCreepMat::computeDeltaCreepStrain(double q_trial, double dt, double t, double eq_cr_old) const {
  // Norton-Bailey: d(eps_cr) / dt = A * q^n
  double rate = A_ * std::pow(q_trial, n_);
  return rate * dt;
}

} // namespace PDCommon::Material
