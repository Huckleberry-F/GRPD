#ifndef PDCOMMON_FRACTURE_DAMAGE_VALUE_FRACTURE_H
#define PDCOMMON_FRACTURE_DAMAGE_VALUE_FRACTURE_H

#include "FractureModel.h"

namespace PDCommon::Fracture {

class DamageValueFracture : public FractureModel {
public:
  DamageValueFracture() = default;
  ~DamageValueFracture() override = default;

  void configure(const YAML::Node &node) override;
  void preCompute(PDCommon::Core::PDContext &ctx, int matId = -1) override;
  void computeFracture(PDCommon::Core::PDContext &ctx, int matId = -1) override;

  std::string getName() const override { return "DamageValueFracture"; }

private:
  double criticalDamage_{1.0};
};

} // namespace PDCommon::Fracture

#endif 
