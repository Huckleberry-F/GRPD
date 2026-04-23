#include "ExplicitImpulseFrictionLaw.h"
#include "ContactRegistry.h"
#include <algorithm>
#include <cmath>

namespace PDCommon::Contact {

void ExplicitImpulseFrictionLaw::initialize(const YAML::Node &configNode) {
  if (configNode["FrictionCoeff"]) {
    frictionCoeff_ = configNode["FrictionCoeff"].as<double>();
  }
}

void ExplicitImpulseFrictionLaw::computeFriction(const ContactContext &pair, 
                                                 double normalForceMag, 
                                                 double dt,
                                                 double &fx, double &fy, double &fz) {
  if (frictionCoeff_ < 1e-6 || normalForceMag <= 0.0 || dt < 1e-30) return;

  // 完全基于多态隔离：透明使用由 Evaluator 计算好的等效相对速度
  double dvx = pair.dvx;
  double dvy = pair.dvy;
  double dvz = pair.dvz;

  double v_rel_n = dvx * pair.nx + dvy * pair.ny + dvz * pair.nz;

  // 切向的相对滑移速度矢量: v_t = dv - (dv * n) * n
  double v_tx = dvx - v_rel_n * pair.nx;
  double v_ty = dvy - v_rel_n * pair.ny;
  double v_tz = dvz - v_rel_n * pair.nz;
  double v_t_mag = std::sqrt(v_tx * v_tx + v_ty * v_ty + v_tz * v_tz);

  if (v_t_mag > 1e-10) {
    // 最大基础摩擦力 F = mu * F_n
    double f_fric_mag = frictionCoeff_ * normalForceMag;

    // [关键限幅保护] 摩擦力不能在一个积分步把相对切向速度减到反向导致震荡
    double m_eff = (pair.mass_i * pair.mass_j) / (pair.mass_i + pair.mass_j);
    double max_fric = m_eff * v_t_mag / dt;
    f_fric_mag = std::min(f_fric_mag, max_fric);

    // 摩擦力方向永远与瞬时切向速度相反，并直接叠加到输出变量
    fx += -f_fric_mag * (v_tx / v_t_mag);
    fy += -f_fric_mag * (v_ty / v_t_mag);
    fz += -f_fric_mag * (v_tz / v_t_mag);
  }
}

} // namespace PDCommon::Contact

REGISTER_FRICTIONLAW_TYPE(ExplicitImpulse, ExplicitImpulseFrictionLaw)
