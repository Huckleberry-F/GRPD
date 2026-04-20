#include "ElasticStickSlipLaw.h"
#include "ContactRegistry.h"
#include <cmath>
#include <algorithm>

namespace PDCommon::Contact {

void ElasticStickSlipLaw::initialize(const YAML::Node &configNode) {
  if (configNode["FrictionCoeff"]) {
    frictionCoeff_ = configNode["FrictionCoeff"].as<double>();
  }
  if (configNode["ShearStiffness"]) {
    shearStiffness_ = configNode["ShearStiffness"].as<double>();
  }
}

void ElasticStickSlipLaw::computeFriction(const ContactContext &pair, 
                                          double normalForceMag, 
                                          double dt,
                                          double &fx, double &fy, double &fz) {
  if (frictionCoeff_ < 1e-6 || normalForceMag <= 0.0 || dt < 1e-30) return;

  uint64_t key = (static_cast<uint64_t>(pair.i) << 32) | static_cast<uint64_t>(pair.j);

  double v_rel_n = pair.dvx * pair.nx + pair.dvy * pair.ny + pair.dvz * pair.nz;

  // 1. 切向滑移速度
  double v_tx = pair.dvx - v_rel_n * pair.nx;
  double v_ty = pair.dvy - v_rel_n * pair.ny;
  double v_tz = pair.dvz - v_rel_n * pair.nz;
  
  // 试探性的摩擦力增量 (K_s * v_t * dt)
  double dfx = shearStiffness_ * (-v_tx) * dt;
  double dfy = shearStiffness_ * (-v_ty) * dt;
  double dfz = shearStiffness_ * (-v_tz) * dt;

  // 2. 查找历史记录并相加得到 Trial Force
  double f_trial_x = dfx;
  double f_trial_y = dfy;
  double f_trial_z = dfz;
  
  auto it = historyTable_.find(key);
  if (it != historyTable_.end()) {
      f_trial_x += it->second[0];
      f_trial_y += it->second[1];
      f_trial_z += it->second[2];
  }

  // 但这个 trial force 不能投影到法向上，所以剔除它可能因为平面旋转产生的法向分量微小误差
  double f_trial_n = f_trial_x * pair.nx + f_trial_y * pair.ny + f_trial_z * pair.nz;
  f_trial_x -= f_trial_n * pair.nx;
  f_trial_y -= f_trial_n * pair.ny;
  f_trial_z -= f_trial_n * pair.nz;

  double f_trial_mag = std::sqrt(f_trial_x * f_trial_x + f_trial_y * f_trial_y + f_trial_z * f_trial_z);
  
  // 3. 库仑屈服限 (Coulomb Yield Limit)
  double f_limit = frictionCoeff_ * normalForceMag;

  if (f_trial_mag > f_limit && f_trial_mag > 1e-12) {
      // 塑性流动 (Slip) - 按照 Radial Return 缩放到极限圆上
      double ratio = f_limit / f_trial_mag;
      f_trial_x *= ratio;
      f_trial_y *= ratio;
      f_trial_z *= ratio;
  } 
  // 如果没有超过 limit，那就是 Stick，完全保留 f_trial

  // 4. 更新历史记录与施加外力
  if (it != historyTable_.end()) {
      it->second[0] = f_trial_x;
      it->second[1] = f_trial_y;
      it->second[2] = f_trial_z;
  } else {
      std::array<double, 3> newHist = {f_trial_x, f_trial_y, f_trial_z};
      historyTable_[key] = newHist;
  }

  // 把计算出的切向阻力叠加到汇总力上
  fx += f_trial_x;
  fy += f_trial_y;
  fz += f_trial_z;
}

} // namespace PDCommon::Contact

REGISTER_FRICTIONLAW_TYPE(ElasticStickSlip, ElasticStickSlipLaw)
