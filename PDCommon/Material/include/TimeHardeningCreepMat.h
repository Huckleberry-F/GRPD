#ifndef PDCOMMON_MATERIAL_TIME_HARDENING_CREEP_MAT_H
#define PDCOMMON_MATERIAL_TIME_HARDENING_CREEP_MAT_H

#include "CreepMaterialBase.h"

namespace PDCommon::Material {

class TimeHardeningCreepMat : public CreepMaterialBase {
public:
  TimeHardeningCreepMat(const std::string &name);
  void initialize(const YAML::Node &matNode) override;

protected:
  double computeDeltaCreepStrain(double q_trial, double dt, double t, double eq_cr_old) const override;
  
private:
  double m_{0.0};
};

} // namespace PDCommon::Material

#endif // PDCOMMON_MATERIAL_TIME_HARDENING_CREEP_MAT_H
