#ifndef PDCOMMON_CONTACT_VISCOUSFORCELAW_H
#define PDCOMMON_CONTACT_VISCOUSFORCELAW_H

// ============================================================================
// ViscousForceLaw.h - 粘性阻尼罚函数接触力学定律
// 公式：F = K*δ + c*δ̇（弹簧力 + 粘性阻尼力）
// ============================================================================

#include "IContactForceLaw.h"
#include <vector>

namespace PDCommon::Contact {

class ViscousForceLaw : public IContactForceLaw {
public:
  ~ViscousForceLaw() override = default;

  std::string getTypeName() const override { return "ViscousForceLaw"; }
  void initialize(const YAML::Node &configNode) override;
  void onPreContact(PDCommon::Core::PDContext &ctx, double maxDx) override;
  ForceResult computeForce(const ContactContext &pair) override;

  void setContactParticleIds(const std::vector<int> &masterIds,
                             const std::vector<int> &slaveIds) {
    masterIds_ = masterIds;
    slaveIds_ = slaveIds;
  }

private:
  double penaltyFactor_ = 1.0;
  double penaltyStiffness_ = -1.0;
  double dampingCoeff_ = 0.2;
  std::vector<int> masterIds_;
  std::vector<int> slaveIds_;
};

} // namespace PDCommon::Contact

#endif // PDCOMMON_CONTACT_VISCOUSFORCELAW_H
