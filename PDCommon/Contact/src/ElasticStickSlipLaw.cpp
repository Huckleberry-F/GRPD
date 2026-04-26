#include "ElasticStickSlipLaw.h"
#include "ContactRegistry.h"
#include "FieldManager.h"
#include "TypedField.h"
#include "PDContext.h"
#include "ParticleManager.h"
#include <cmath>

namespace PDCommon::Contact {

void ElasticStickSlipLaw::initialize(const YAML::Node &configNode) {
  if (configNode["FrictionCoeff"]) {
    frictionCoeff_ = configNode["FrictionCoeff"].as<double>();
  }
  if (configNode["ShearStiffness"]) {
    shearStiffness_ = configNode["ShearStiffness"].as<double>();
  }
  if (configNode["AllowableSlipRatio"]) {
    allowableSlipRatio_ = configNode["AllowableSlipRatio"].as<double>();
  }
  if (configNode["StiffnessMode"]) {
    stiffnessMode_ = configNode["StiffnessMode"].as<int>();
  }
  if (configNode["AnsysKTNRatio"]) {
    ansysKTNRatio_ = configNode["AnsysKTNRatio"].as<double>();
  }
}

void ElasticStickSlipLaw::onPreContact(PDCommon::Core::PDContext &ctx) {
  auto &fm = ctx.getFieldManager();
  numParticles_ = ctx.getParticleManager().getTotalParticles();

  // -----------------------------------------------------------------------
  // 1. 按需注册切向力场 (FrictionShearForce, double, dim=3)
  // -----------------------------------------------------------------------
  auto *shearField = fm.getFieldAs<double>("FrictionShearForce");
  if (!shearField) {
    auto newField = std::make_unique<PDCommon::Field::TypedField<double>>(
        "FrictionShearForce", 3);
    newField->resize(numParticles_);
    fm.addField(std::move(newField));
    shearField = fm.getFieldAs<double>("FrictionShearForce");
  }
  shearForceData_ = shearField ? shearField->dataPtr() : nullptr;

  // -----------------------------------------------------------------------
  // 2. 按需注册接触活跃标记场 (FrictionContactActive, int, dim=1)
  // -----------------------------------------------------------------------
  auto *activeField = fm.getFieldAs<int>("FrictionContactActive");
  if (!activeField) {
    auto newField = std::make_unique<PDCommon::Field::TypedField<int>>(
        "FrictionContactActive", 1);
    newField->resize(numParticles_);
    fm.addField(std::move(newField));
    activeField = fm.getFieldAs<int>("FrictionContactActive");
  }
  contactActiveData_ = activeField ? activeField->dataPtr() : nullptr;

  // -----------------------------------------------------------------------
  // 3. 核心清理逻辑：对上一步未接触的粒子，清零其切向力历史
  //    然后将所有标记重置为 0，等待本步 computeFriction 重新激活
  // -----------------------------------------------------------------------
  if (shearForceData_ && contactActiveData_) {
#pragma omp parallel for schedule(static)
    for (long long i = 0; i < static_cast<long long>(numParticles_); ++i) {
      if (contactActiveData_[i] == 0) {
        // 该粒子上一步没有接触 → 清零残留的切向力历史
        shearForceData_[i * 3] = 0.0;
        shearForceData_[i * 3 + 1] = 0.0;
        shearForceData_[i * 3 + 2] = 0.0;
      }
      // 重置标记为 0，等待本步 computeFriction 激活
      contactActiveData_[i] = 0;
    }
  }
}

void ElasticStickSlipLaw::computeFriction(const ContactContext &pair,
                                          double normalForceMag, double dt,
                                          double &fx, double &fy, double &fz) {
  // 守卫：无摩擦系数、无法向力、无场指针时直接返回
  if (frictionCoeff_ < 1e-6 || normalForceMag <= 0.0 || dt < 1e-30)
    return;
  if (!shearForceData_ || !contactActiveData_)
    return;

  // 标记该 Slave 粒子本步处于接触状态（防止下一步 onPreContact 误清零）
  contactActiveData_[pair.i] = 1;

  // 提取法向投影分量，用于分离切向相对速度
  double v_rel_n = pair.dvx * pair.nx + pair.dvy * pair.ny + pair.dvz * pair.nz;

  // 1. 切向滑移速度（剔除法向分量后的纯切向相对运动）
  double v_tx = pair.dvx - v_rel_n * pair.nx;
  double v_ty = pair.dvy - v_rel_n * pair.ny;
  double v_tz = pair.dvz - v_rel_n * pair.nz;

  // 动态计算切向刚度
  double K_s = shearStiffness_;
  if (K_s <= 0.0) {
      if (stiffnessMode_ == 0) {
          // Abaqus 风格：基于允许弹性滑移量
          double allowable_slip = allowableSlipRatio_ * pair.dx_i;
          if (allowable_slip > 1e-12) {
              K_s = (frictionCoeff_ * normalForceMag) / allowable_slip;
          } else {
              K_s = 1.0e6; // 降级保护
          }
      } else if (stiffnessMode_ == 1) {
          // Ansys 风格：KTS = ansysKTNRatio * KTN
          // 法向刚度 K_n 约等于法向力除以物理穿透量
          if (pair.raw_penetration > 1e-12) {
              K_s = ansysKTNRatio_ * (normalForceMag / pair.raw_penetration);
          } else {
              K_s = 1.0e6; // 降级保护
          }
      } else {
          K_s = 1.0e6; // 未知模式
      }
  }

  // 试探性的摩擦力增量 (K_s * v_t * dt)
  double dfx = K_s * (-v_tx) * dt;
  double dfy = K_s * (-v_ty) * dt;
  double dfz = K_s * (-v_tz) * dt;

  // 2. 从 Slave 粒子的历史状态场中读取上一步累积的切向力，叠加增量得到 Trial Force
  int idx3 = pair.i * 3;
  double f_trial_x = dfx + shearForceData_[idx3];
  double f_trial_y = dfy + shearForceData_[idx3 + 1];
  double f_trial_z = dfz + shearForceData_[idx3 + 2];

  // 正交投影：剔除 trial force 在当前法向上的分量误差
  // （当 Slave 滑过不同法向的 Master 表面时，旧的历史力可能含有新法向的分量）
  double f_trial_n =
      f_trial_x * pair.nx + f_trial_y * pair.ny + f_trial_z * pair.nz;
  f_trial_x -= f_trial_n * pair.nx;
  f_trial_y -= f_trial_n * pair.ny;
  f_trial_z -= f_trial_n * pair.nz;

  double f_trial_mag = std::sqrt(f_trial_x * f_trial_x + f_trial_y * f_trial_y +
                                 f_trial_z * f_trial_z);

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

  // 4. 将更新后的切向力写回 Slave 粒子的状态场
  //    注意：GeneralContact 按 slaveId 展开 OpenMP 并行循环，
  //    每个 slave 只被一个线程处理，因此 pair.i 的写入天然线程安全。
  shearForceData_[idx3] = f_trial_x;
  shearForceData_[idx3 + 1] = f_trial_y;
  shearForceData_[idx3 + 2] = f_trial_z;

  // 把计算出的切向阻力叠加到汇总力上
  fx += f_trial_x;
  fy += f_trial_y;
  fz += f_trial_z;
}

} // namespace PDCommon::Contact

REGISTER_FRICTIONLAW_TYPE(ElasticStickSlip, ElasticStickSlipLaw)
