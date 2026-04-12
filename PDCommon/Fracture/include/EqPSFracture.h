#ifndef PDCOMMON_FRACTURE_EQPS_FRACTURE_H
#define PDCOMMON_FRACTURE_EQPS_FRACTURE_H

#include "FractureModel.h"

namespace PDCommon::Fracture {

class EqPSFracture : public FractureModel {
public:
  EqPSFracture() = default;
  ~EqPSFracture() override = default;

  void configure(const YAML::Node &node) override;
  void preCompute(PDCommon::Core::PDContext &ctx, int matId = -1) override;
  void computeFracture(PDCommon::Core::PDContext &ctx, int matId = -1) override;

  std::string getName() const override { return "EqPSFracture"; }

private:
  double criticalEqPS_{0.1};
  double criticalDamage_{0.8}; // 【新增】宏观断裂处决阈值（默认 80% 键断裂即失活）
};

} // namespace PDCommon::Fracture

#endif 
