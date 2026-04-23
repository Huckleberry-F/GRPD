#ifndef PDCOMMON_CONTACT_EXPLICITIMPULSEFRICTIONLAW_H
#define PDCOMMON_CONTACT_EXPLICITIMPULSEFRICTIONLAW_H

#include "IFrictionLaw.h"

namespace PDCommon::Contact {

class ExplicitImpulseFrictionLaw : public IFrictionLaw {
public:
  ~ExplicitImpulseFrictionLaw() override = default;

  std::string getTypeName() const override { return "ExplicitImpulse"; }
  void initialize(const YAML::Node &configNode) override;

  void computeFriction(const ContactContext &pair, 
                       double normalForceMag, 
                       double dt,
                       double &fx, double &fy, double &fz) override;

private:
  double frictionCoeff_ = 0.0;
};

} // namespace PDCommon::Contact

#endif // PDCOMMON_CONTACT_EXPLICITIMPULSEFRICTIONLAW_H
