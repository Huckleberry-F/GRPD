#include "KinematicForceLaw.h"
#include "ContactRegistry.h"
#include "PDContext.h"
#include <algorithm>

namespace PDCommon::Contact {

void KinematicForceLaw::initialize(const YAML::Node &configNode) {
  if (configNode["RestitutionCoeff"]) {
    restitutionCoeff_ = configNode["RestitutionCoeff"].as<double>();
  }
}

void KinematicForceLaw::onPreContact(PDCommon::Core::PDContext &ctx,
                                     double maxDx) {
  dt_ = ctx.getCurrentDt();
  if (dt_ < 1e-30) {
    dt_ = 1e-9;
  }
}

ForceResult KinematicForceLaw::computeForce(const ContactContext &pair) {
  ForceResult res;
  res.valid = false;

  if (pair.raw_penetration <= 0.0)
    return res;

  // 等效质量
  double m_eff = (pair.mass_i * pair.mass_j) / (pair.mass_i + pair.mass_j);

  // 位置修正力
  double f_pos = m_eff * pair.raw_penetration / (dt_ * dt_);
  // 速度阻尼力
  double f_damp = 0.0;

  // 完全基于多态隔离：不管底层的 dv 是 NTN 算出来的，还是 NTC
  // 加权出来的，这里透明使用！
  double v_rel_n = pair.dvx * pair.nx + pair.dvy * pair.ny + pair.dvz * pair.nz;
  if (v_rel_n < 0.0) { // 逼近
    f_damp = (1.0 + restitutionCoeff_) * m_eff * (-v_rel_n) / dt_;
    f_damp = std::min(f_damp, 2.0 * f_pos); // 限幅保护
  }

  double force_mag = f_pos + f_damp;
  res.fx = force_mag * pair.nx;
  res.fy = force_mag * pair.ny;
  res.fz = force_mag * pair.nz;
  res.valid = true;

  return res;
}

} // namespace PDCommon::Contact

REGISTER_FORCELAW_TYPE(KinematicNormal, KinematicForceLaw)
