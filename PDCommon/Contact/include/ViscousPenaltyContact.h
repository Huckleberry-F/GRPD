#ifndef PDCOMMON_CONTACT_VISCOUSPENALTYCONTACT_H
#define PDCOMMON_CONTACT_VISCOUSPENALTYCONTACT_H

// ============================================================================
// ViscousPenaltyContact.h - 粘性阻尼罚函数接触算法
// 特点：在弹簧刚度基础上，加入基于相对速度的粘性阻尼力 F = k*δ + c*δ̇
// ============================================================================

#include "NodeNodeContact.h"

namespace PDCommon::Contact {

class ViscousPenaltyContact : public NodeNodeContact {
public:
  explicit ViscousPenaltyContact(const std::string &name = "ViscousPenaltyContact");
  ~ViscousPenaltyContact() override = default;

  std::string getTypeName() const override { return "ViscousPenaltyContact"; }
  void initialize(const YAML::Node &configNode) override;

protected:
  void onPreContact(PDCommon::Core::PDContext &ctx, double maxDx) override;
  ContactPairResult computePairForce(const ContactPairContext &pair) override;

private:
  double penaltyFactor_ = 1.0;
  double penaltyStiffness_ = -1.0;
  double dampingCoeff_ = 0.2;
};

} // namespace PDCommon::Contact

#endif // PDCOMMON_CONTACT_VISCOUSPENALTYCONTACT_H
