#include "TanhFrictionLaw.h"
#include "ContactRegistry.h"
#include <cmath>
#include <algorithm>

namespace PDCommon::Contact {

void TanhFrictionLaw::initialize(const YAML::Node &configNode) {
  if (configNode["FrictionCoeff"]) {
    frictionCoeff_ = configNode["FrictionCoeff"].as<double>();
  }
  if (configNode["VRef"]) {
    vRef_ = configNode["VRef"].as<double>();
  }
}

void TanhFrictionLaw::computeFriction(const ContactContext &pair, 
                                      double normalForceMag, 
                                      double dt,
                                      double &fx, double &fy, double &fz) {
  if (frictionCoeff_ < 1e-6 || normalForceMag <= 0.0) return;

  double v_rel_n = pair.dvx * pair.nx + pair.dvy * pair.ny + pair.dvz * pair.nz;

  // 切向的相对滑移速度矢量: v_t = dv - v_rel_n * n
  double v_tx = pair.dvx - v_rel_n * pair.nx;
  double v_ty = pair.dvy - v_rel_n * pair.ny;
  double v_tz = pair.dvz - v_rel_n * pair.nz;
  double v_t_mag = std::sqrt(v_tx * v_tx + v_ty * v_ty + v_tz * v_tz);

  if (v_t_mag > 1e-12) {
    // 双曲正切平滑公式
    double smoothFactor = std::tanh(v_t_mag / vRef_);
    double f_fric_mag = frictionCoeff_ * normalForceMag * smoothFactor;

    // 摩擦力方向永远与瞬时切向速度相反
    fx += -f_fric_mag * (v_tx / v_t_mag);
    fy += -f_fric_mag * (v_ty / v_t_mag);
    fz += -f_fric_mag * (v_tz / v_t_mag);
  }
}

} // namespace PDCommon::Contact

REGISTER_FRICTIONLAW_TYPE(TanhRegularized, TanhFrictionLaw)
