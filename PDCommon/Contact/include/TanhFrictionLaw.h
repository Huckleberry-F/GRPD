#pragma once

#include "IFrictionLaw.h"
#include <yaml-cpp/yaml.h>

namespace PDCommon::Contact {

class TanhFrictionLaw : public IFrictionLaw {
public:
  TanhFrictionLaw() = default;
  ~TanhFrictionLaw() override = default;

  void initialize(const YAML::Node &configNode) override;

  std::string getTypeName() const override { return "TanhRegularized"; }

  void computeFriction(const ContactContext &pair, 
                       double normalForceMag, 
                       double dt,
                       double &fx, double &fy, double &fz) override;

private:
  double frictionCoeff_ = 0.0;
  double vRef_ = 1.0e-3; // 参考速度
};

} // namespace PDCommon::Contact
