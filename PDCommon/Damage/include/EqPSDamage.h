#ifndef PDCOMMON_DAMAGE_EQPS_DAMAGE_H
#define PDCOMMON_DAMAGE_EQPS_DAMAGE_H

// ============================================================================
// EqPSDamage.h - 基于等效塑性应变的损伤模型
// ============================================================================

#include "DamageModel.h"
#include <string>
#include <vector>

namespace PDCommon::Damage {

class EqPSDamage : public DamageModel {
public:
  EqPSDamage() = default;
  ~EqPSDamage() override = default;

  void configure(const YAML::Node &node) override;
  void preCompute(PDCommon::Core::PDContext &ctx, int matId = -1) override;
  void computeDamage(PDCommon::Core::PDContext &ctx, int matId = -1) override;

  std::string getName() const override { return "EqPlasticStrain"; }

private:
  double criticalEqPS_{0.15};

};

} // namespace PDCommon::Damage

#endif // PDCOMMON_DAMAGE_EQPS_DAMAGE_H
