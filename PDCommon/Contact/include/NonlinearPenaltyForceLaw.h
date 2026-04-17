#ifndef PDCOMMON_CONTACT_NONLINEARPENALTYFORCELAW_H
#define PDCOMMON_CONTACT_NONLINEARPENALTYFORCELAW_H

// ============================================================================
// NonlinearPenaltyForceLaw.h - 非线性渐进罚函数接触力学定律
// ============================================================================

#include "IContactForceLaw.h"
#include <vector>

namespace PDCommon::Contact {

class NonlinearPenaltyForceLaw : public IContactForceLaw {
public:
  ~NonlinearPenaltyForceLaw() override = default;

  std::string getTypeName() const override {
    return "NonlinearPenaltyForceLaw";
  }
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
  double nonlinearOnsetRatio_ = 0.5;
  double stiffeningCoeff_ = 20.0;
  std::vector<int> masterIds_;
  std::vector<int> slaveIds_;
};

} // namespace PDCommon::Contact

#endif // PDCOMMON_CONTACT_NONLINEARPENALTYFORCELAW_H
