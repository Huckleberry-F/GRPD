// ============================================================================
// BondStretchFracture.h - 基于临界伸长率的脆性断裂判据
// ============================================================================

#ifndef PDCOMMON_FRACTURE_CRITICAL_STRETCH_H
#define PDCOMMON_FRACTURE_CRITICAL_STRETCH_H

#include "FractureModel.h"

namespace PDCommon::Fracture {

/// @brief 原型键的临界伸长率断裂模型
class BondStretchFracture : public FractureModel {
public:
  BondStretchFracture() = default;
  ~BondStretchFracture() override = default;

  void configure(const YAML::Node &node) override;
  void preCompute(PDCommon::Core::PDContext &ctx, int matId = -1) override;
  void computeFracture(PDCommon::Core::PDContext &ctx, int matId = -1) override;
  
  std::string getName() const override { return "BondStretchFracture"; }

private:
  double criticalStretch_{0.0};
};

} // namespace PDCommon::Fracture

#endif // PDCOMMON_FRACTURE_CRITICAL_STRETCH_H
