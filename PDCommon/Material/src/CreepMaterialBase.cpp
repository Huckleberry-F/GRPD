#include "CreepMaterialBase.h"
#include "FieldManager.h"
#include "PDContext.h"
#include "FieldRegistry.h"
#include "Logger.h"
#include <cmath>

namespace PDCommon::Material {

void CreepMaterialBase::initialize(const YAML::Node &matNode) {
  if (matNode["Density"])
    density_ = matNode["Density"].as<double>();
  if (matNode["YoungsModulus"])
    E_ = matNode["YoungsModulus"].as<double>();
  if (matNode["PoissonsRatio"])
    nu_ = matNode["PoissonsRatio"].as<double>();
  if (matNode["LargeDeformation"])
    largeDeformation_ = matNode["LargeDeformation"].as<bool>();

  if (matNode["A"])
    A_ = matNode["A"].as<double>();
  if (matNode["n"])
    n_ = matNode["n"].as<double>();

  mu_ = E_ / (2.0 * (1.0 + nu_));
  lambda_ = (E_ * nu_) / ((1.0 + nu_) * (1.0 - 2.0 * nu_));
  bulkModulus_ = E_ / (3.0 * (1.0 - 2.0 * nu_));
}

void CreepMaterialBase::allocateStateVariables(PDCommon::Field::FieldManager &fm) {
  auto &reg = PDCommon::Field::FieldRegistry::getInstance();

  fm.addField(reg.createField("DoubleField", "EqCreepStrain_Old", 1));
  fm.addField(reg.createField("DoubleField", "EqCreepStrain_Trial", 1));
  fm.registerSwapPair("EqCreepStrain_Old", "EqCreepStrain_Trial");

  fm.addField(reg.createField("DoubleField", "CreepStrain_Old", 9));
  fm.addField(reg.createField("DoubleField", "CreepStrain_Trial", 9));
  fm.registerSwapPair("CreepStrain_Old", "CreepStrain_Trial");
}

void CreepMaterialBase::bindStateVariables(PDCommon::Field::FieldManager &fm) {
  fieldEqCrOld_ = fm.getFieldAs<double>("EqCreepStrain_Old");
  fieldEqCrTrial_ = fm.getFieldAs<double>("EqCreepStrain_Trial");
  fieldCrOld_ = fm.getFieldAs<double>("CreepStrain_Old");
  fieldCrTrial_ = fm.getFieldAs<double>("CreepStrain_Trial");

  if (fieldEqCrOld_ && fieldEqCrTrial_ && fieldCrOld_ && fieldCrTrial_) {
    eqCrOld_ = fieldEqCrOld_->dataPtr();
    eqCrTrial_ = fieldEqCrTrial_->dataPtr();
    crOld_ = fieldCrOld_->dataPtr();
    crTrial_ = fieldCrTrial_->dataPtr();
  }
}

void CreepMaterialBase::commitState() {
  if (!fieldEqCrOld_ || !fieldEqCrTrial_ || !fieldCrOld_ || !fieldCrTrial_)
    return;

  eqCrOld_ = fieldEqCrOld_->dataPtr();
  eqCrTrial_ = fieldEqCrTrial_->dataPtr();
  crOld_ = fieldCrOld_->dataPtr();
  crTrial_ = fieldCrTrial_->dataPtr();
}

Eigen::Matrix3d CreepMaterialBase::ComputeEngineeringStress(const Eigen::Matrix3d &strain, PDCommon::Core::PDContext *ctx) const {
  double e_trace = strain.trace();
  return 2.0 * mu_ * strain + lambda_ * e_trace * Eigen::Matrix3d::Identity();
}

Eigen::Matrix3d CreepMaterialBase::ComputePK1Stress(const Eigen::Matrix3d &F, int particleId, int stateMode, PDCommon::Core::PDContext *ctx) const {
  if (particleId < 0 || eqCrOld_ == nullptr || ctx == nullptr) {
    Eigen::Matrix3d strain = 0.5 * (F + F.transpose()) - Eigen::Matrix3d::Identity();
    return ComputeEngineeringStress(strain, ctx);
  }

  Eigen::Matrix3d I = Eigen::Matrix3d::Identity();
  Eigen::Matrix3d eps = Eigen::Matrix3d::Zero();

  if (largeDeformation_) {
    eps = 0.5 * (F.transpose() * F - I);
  } else {
    eps = 0.5 * (F + F.transpose()) - I;
  }

  int idx9 = particleId * 9;

  if (stateMode == 1 || stateMode == 2) {
    Eigen::Matrix3d eps_cr_fixed = Eigen::Matrix3d::Zero();
    const double* cr_ptr = (stateMode == 1) ? crOld_ : crTrial_;

    eps_cr_fixed << cr_ptr[idx9], cr_ptr[idx9 + 1], cr_ptr[idx9 + 2],
        cr_ptr[idx9 + 3], cr_ptr[idx9 + 4], cr_ptr[idx9 + 5], cr_ptr[idx9 + 6],
        cr_ptr[idx9 + 7], cr_ptr[idx9 + 8];

    Eigen::Matrix3d eps_e = eps - eps_cr_fixed;
    double e_trace = eps_e.trace();
    Eigen::Matrix3d S_pred = 2.0 * mu_ * eps_e + lambda_ * e_trace * I;

    if (largeDeformation_) {
      return F * S_pred;
    }
    return S_pred;
  }

  // 正常状态 (stateMode == 0)
  Eigen::Matrix3d eps_cr_old = Eigen::Matrix3d::Zero();
  eps_cr_old << crOld_[idx9], crOld_[idx9 + 1], crOld_[idx9 + 2],
      crOld_[idx9 + 3], crOld_[idx9 + 4], crOld_[idx9 + 5], crOld_[idx9 + 6],
      crOld_[idx9 + 7], crOld_[idx9 + 8];

  double eq_cr_old = eqCrOld_[particleId];

  // 试探弹性应变
  Eigen::Matrix3d eps_e_trial = eps - eps_cr_old;
  double tr_eps_e = eps_e_trial.trace();

  // 偏应变
  Eigen::Matrix3d e_e_trial = eps_e_trial - (tr_eps_e / 3.0) * I;

  // 试探偏应力
  Eigen::Matrix3d S_trial = 2.0 * mu_ * e_e_trial;

  double hydro_stress = bulkModulus_ * tr_eps_e;

  // Von Mises 应力
  double norm_S = std::sqrt((S_trial.cwiseProduct(S_trial)).sum());
  double q_trial = std::sqrt(1.5) * norm_S;

  // 提取时间信息
  double dt = ctx->getCurrentDt();
  double t = ctx->getCurrentTime();

  double delta_eq_cr = 0.0;
  Eigen::Matrix3d S_new = S_trial;
  Eigen::Matrix3d eps_cr_new = eps_cr_old;

  if (q_trial > 1e-12) {
    delta_eq_cr = computeDeltaCreepStrain(q_trial, dt, t, eq_cr_old);

    // Radial return for creep
    double factor = 3.0 * mu_ * delta_eq_cr / q_trial;
    S_new = S_trial * (1.0 - factor);

    Eigen::Matrix3d N = 1.5 * S_trial / q_trial;
    eps_cr_new += delta_eq_cr * N;
  }

  double eq_cr_new = eq_cr_old + delta_eq_cr;

  // 保存 Trial 态
  eqCrTrial_[particleId] = eq_cr_new;
  crTrial_[idx9] = eps_cr_new(0, 0);
  crTrial_[idx9 + 1] = eps_cr_new(0, 1);
  crTrial_[idx9 + 2] = eps_cr_new(0, 2);
  crTrial_[idx9 + 3] = eps_cr_new(1, 0);
  crTrial_[idx9 + 4] = eps_cr_new(1, 1);
  crTrial_[idx9 + 5] = eps_cr_new(1, 2);
  crTrial_[idx9 + 6] = eps_cr_new(2, 0);
  crTrial_[idx9 + 7] = eps_cr_new(2, 1);
  crTrial_[idx9 + 8] = eps_cr_new(2, 2);

  Eigen::Matrix3d sigma = S_new + hydro_stress * I;

  if (largeDeformation_) {
    return F * sigma;
  }
  return sigma;
}

} // namespace PDCommon::Material
