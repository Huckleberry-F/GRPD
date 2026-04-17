#ifndef PDCOMMON_CONTACT_PENALTYFORCELAW_H
#define PDCOMMON_CONTACT_PENALTYFORCELAW_H

// ============================================================================
// PenaltyForceLaw.h - 线性罚函数接触力学定律
// 公式：F = K * effective_penetration（含 Pinball 截断保护）
// ============================================================================

#include "IContactForceLaw.h"

namespace PDCommon::Contact {

class PenaltyForceLaw : public IContactForceLaw {
public:
  ~PenaltyForceLaw() override = default;

  std::string getTypeName() const override { return "PenaltyForceLaw"; }
  void initialize(const YAML::Node &configNode) override;
  void onPreContact(PDCommon::Core::PDContext &ctx, double maxDx) override;
  ForceResult computeForce(const ContactContext &pair) override;

  // ---- 供 StandardContactAlgorithm 在组装阶段注入 MasterIds/SlaveIds 用 ----
  void setContactParticleIds(const std::vector<int> &masterIds,
                             const std::vector<int> &slaveIds) {
    masterIds_ = masterIds;
    slaveIds_ = slaveIds;
  }

private:
  double penaltyFactor_ = 1.0;
  double penaltyStiffness_ = -1.0;
  double pinballRatio_ = 0.5;

  // 为 onPreContact 自动估算刚度保留（需知道哪些粒子属于主从面）
  std::vector<int> masterIds_;
  std::vector<int> slaveIds_;
};

} // namespace PDCommon::Contact

#endif // PDCOMMON_CONTACT_PENALTYFORCELAW_H
