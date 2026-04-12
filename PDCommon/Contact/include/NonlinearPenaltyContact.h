#ifndef PDCOMMON_CONTACT_NONLINEARPENALTYCONTACT_H
#define PDCOMMON_CONTACT_NONLINEARPENALTYCONTACT_H

// ============================================================================
// NonlinearPenaltyContact.h - 非线性渐进罚函数接触算法
// ============================================================================

#include "NodeNodeContact.h"

namespace PDCommon::Contact {

class NonlinearPenaltyContact : public NodeNodeContact {
public:
  explicit NonlinearPenaltyContact(
      const std::string &name = "NonlinearPenaltyContact");
  ~NonlinearPenaltyContact() override = default;

  std::string getTypeName() const override { return "NonlinearPenaltyContact"; }
  void initialize(const YAML::Node &configNode) override;

protected:
  void onPreContact(PDCommon::Core::PDContext &ctx, double maxDx) override;
  ContactPairResult computePairForce(const ContactPairContext &pair) override;

private:
  double penaltyFactor_ = 1.0;
  double penaltyStiffness_ = -1.0;
  double nonlinearOnsetRatio_ = 0.5;
  double stiffeningCoeff_ = 20.0;
};

} // namespace PDCommon::Contact

#endif // PDCOMMON_CONTACT_NONLINEARPENALTYCONTACT_H
