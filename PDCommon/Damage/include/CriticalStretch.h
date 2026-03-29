// ============================================================================
// CriticalStretch.h - 基于临界伸长率的脆性断裂判据
// ============================================================================

#ifndef PDCOMMON_DAMAGE_CRITICAL_STRETCH_H
#define PDCOMMON_DAMAGE_CRITICAL_STRETCH_H

#include "DamageModel.h"

namespace PDCommon::Damage {

/// @brief 原型键的临界伸长率断裂模型
class CriticalStretch : public DamageModel {
public:
  CriticalStretch() = default;
  ~CriticalStretch() override = default;

  void configure(const YAML::Node &node) override;
  void preCompute(PDCommon::Core::PDContext &ctx, int matId = -1) override;
  void computeDamage(PDCommon::Core::PDContext &ctx, int matId = -1) override;
  
  std::string getName() const override { return "CriticalStretch"; }

private:
  double criticalStretch_{0.0};
  
  // 用于统计初始每颗粒子的总键数，用以计算最终 Damage 指数
  std::vector<int> initialBondsCount_; 
};

} // namespace PDCommon::Damage

#endif // PDCOMMON_DAMAGE_CRITICAL_STRETCH_H
