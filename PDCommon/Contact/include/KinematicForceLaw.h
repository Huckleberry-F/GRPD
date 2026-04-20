#ifndef PDCOMMON_CONTACT_KINEMATICFORCELAW_H
#define PDCOMMON_CONTACT_KINEMATICFORCELAW_H

#include "IContactForceLaw.h"
#include <vector>

namespace PDCommon::Contact {

class KinematicForceLaw : public IContactForceLaw {
public:
  ~KinematicForceLaw() override = default;

  std::string getTypeName() const override { return "KinematicForceLaw"; }
  void initialize(const YAML::Node &configNode) override;
  void onPreContact(PDCommon::Core::PDContext &ctx, double maxDx) override;
  ForceResult computeForce(const ContactContext &pair) override;

  void setContactParticleIds(const std::vector<int> &masterIds,
                             const std::vector<int> &slaveIds) override {
    masterIds_ = masterIds;
    slaveIds_ = slaveIds;
  }

private:
  double restitutionCoeff_ = 0.0;
  double dt_ = 1e-9;
  std::vector<int> masterIds_;
  std::vector<int> slaveIds_;
};

} // namespace PDCommon::Contact

#endif // PDCOMMON_CONTACT_KINEMATICFORCELAW_H
