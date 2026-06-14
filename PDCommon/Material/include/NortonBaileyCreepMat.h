#ifndef PDCOMMON_MATERIAL_NORTON_BAILEY_CREEP_MAT_H
#define PDCOMMON_MATERIAL_NORTON_BAILEY_CREEP_MAT_H

#include "CreepMaterialBase.h"

namespace PDCommon::Material {

class NortonBaileyCreepMat : public CreepMaterialBase {
public:
  NortonBaileyCreepMat(const std::string &name);

protected:
  double computeDeltaCreepStrain(double q_trial, double dt, double t, double eq_cr_old) const override;
};

} // namespace PDCommon::Material

#endif // PDCOMMON_MATERIAL_NORTON_BAILEY_CREEP_MAT_H
