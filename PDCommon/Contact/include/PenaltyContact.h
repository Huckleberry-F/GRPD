#ifndef PDCOMMON_CONTACT_PENALTYCONTACT_H
#define PDCOMMON_CONTACT_PENALTYCONTACT_H

// ============================================================================
// PenaltyContact.h - 惩罚函数接触算法（含 Pinball 截断）
// ============================================================================

#include "NodeNodeContact.h"

namespace PDCommon::Contact {

class PenaltyContact : public NodeNodeContact {
public:
  explicit PenaltyContact(const std::string &name = "PenaltyContact");
  ~PenaltyContact() override = default;

  std::string getTypeName() const override { return "PenaltyContact"; }
  void initialize(const YAML::Node &configNode) override;

protected:
  void onPreContact(PDCommon::Core::PDContext &ctx, double maxDx) override;
  ContactPairResult computePairForce(const ContactPairContext &pair) override;

private:
  double penaltyFactor_ = 1.0;
  double penaltyStiffness_ = -1.0;
  double pinballRatio_ = 0.5;
};

} // namespace PDCommon::Contact

#endif // PDCOMMON_CONTACT_PENALTYCONTACT_H
