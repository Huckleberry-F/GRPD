#ifndef PDCOMMON_CONTACT_SILLINGFORCELAW_H
#define PDCOMMON_CONTACT_SILLINGFORCELAW_H

// ============================================================================
// SillingForceLaw.h - Silling 短程排斥力接触力学定律
// 参考：Silling & Askari (2005)
// ============================================================================

#include "IContactForceLaw.h"
#include <vector>

namespace PDCommon::Contact {

class SillingForceLaw : public IContactForceLaw {
public:
  ~SillingForceLaw() override = default;

  std::string getTypeName() const override { return "SillingForceLaw"; }
  void initialize(const YAML::Node &configNode) override;
  void onPreContact(PDCommon::Core::PDContext &ctx, double maxDx) override;
  ForceResult computeForce(const ContactContext &pair) override;

  void setParticleIds(const std::vector<int> &masterIds,
                      const std::vector<int> &slaveIds) {
    masterIds_ = masterIds;
    slaveIds_ = slaveIds;
  }

private:
  double shortRangeStiffness_ = -1.0;
  double stiffnessFactor_ = 1.0;
  int dim_ = 3;
  double effectiveThickness_ = 1.0;
  std::vector<int> masterIds_;
  std::vector<int> slaveIds_;
};

} // namespace PDCommon::Contact

#endif // PDCOMMON_CONTACT_SILLINGFORCELAW_H
