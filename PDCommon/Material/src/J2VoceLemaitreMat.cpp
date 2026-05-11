#include "J2VoceLemaitreMat.h"
#include "FieldManager.h"
#include "FieldRegistry.h"
#include "Logger.h"
#include "MaterialRegistry.h"
#include <cmath>

namespace PDCommon::Material {

// ---------------------------------------------------------------------------
// 向工厂注册材料类型
// ---------------------------------------------------------------------------
REGISTER_MATERIAL_TYPE(J2VoceLemaitre, [](const std::string &name) {
  return std::make_unique<PDCommon::Material::J2VoceLemaitreMat>(name);
});

J2VoceLemaitreMat::J2VoceLemaitreMat(const std::string &name)
    : MechanicalMaterial(name) {}

void J2VoceLemaitreMat::initialize(const YAML::Node &matNode) {
  MechanicalMaterial::initialize(matNode);

  if (matNode["Density"])
    density_ = matNode["Density"].as<double>();
  if (matNode["YoungsModulus"])
    youngsModulus_ = matNode["YoungsModulus"].as<double>();
  if (matNode["PoissonsRatio"])
    poissonsRatio_ = matNode["PoissonsRatio"].as<double>();

  if (matNode["YieldStress"])
    yieldStress_ = matNode["YieldStress"].as<double>();
  if (matNode["LinearHardening"])
    linearHardening_ = matNode["LinearHardening"].as<double>();
  if (matNode["Voce_R"])
    voceR_ = matNode["Voce_R"].as<double>();
  if (matNode["Voce_b"])
    voceB_ = matNode["Voce_b"].as<double>();
    
  if (matNode["Lemaitre_S"])
    lemaitreS_ = matNode["Lemaitre_S"].as<double>();
  if (matNode["Lemaitre_s"])
    lemaitre_s_ = matNode["Lemaitre_s"].as<double>();
  if (matNode["DamageThreshold"])
    damageThreshold_ = matNode["DamageThreshold"].as<double>();
  if (matNode["CriticalDamage"])
    criticalDamage_ = matNode["CriticalDamage"].as<double>();
  if (matNode["DamageAccelThreshold"])
    damageAccelThreshold_ = matNode["DamageAccelThreshold"].as<double>();
  if (matNode["DamageAccelFactor"])
    damageAccelFactor_ = matNode["DamageAccelFactor"].as<double>();

  if (matNode["LargeDeformation"])
    largeDeformation_ = matNode["LargeDeformation"].as<bool>();

  if (density_ <= 0.0 || youngsModulus_ <= 0.0 || poissonsRatio_ < 0.0 ||
      poissonsRatio_ > 0.5) {
    LOG_ERROR(
        "[J2VoceLemaitreMat] Invalid basic elastic parameters for material: " +
        name_);
    exit(EXIT_FAILURE);
  }

  computeLameParameters();
}

void J2VoceLemaitreMat::computeLameParameters() {
  lambda_ = (youngsModulus_ * poissonsRatio_) /
            ((1.0 + poissonsRatio_) * (1.0 - 2.0 * poissonsRatio_));
  mu_ = youngsModulus_ / (2.0 * (1.0 + poissonsRatio_));
  bulkModulus_ = youngsModulus_ / (3.0 * (1.0 - 2.0 * poissonsRatio_));
}

void J2VoceLemaitreMat::allocateStateVariables(
    PDCommon::Field::FieldManager &fm) {
  auto &reg = PDCommon::Field::FieldRegistry::getInstance();

  fm.addField(reg.createField("DoubleField", "EqPlasticStrain_Old", 1));
  fm.addField(reg.createField("DoubleField", "EqPlasticStrain_Trial", 1));
  fm.registerSwapPair("EqPlasticStrain_Old", "EqPlasticStrain_Trial");

  fm.addField(reg.createField("DoubleField", "PlasticStrain_Old", 9));
  fm.addField(reg.createField("DoubleField", "PlasticStrain_Trial", 9));
  fm.registerSwapPair("PlasticStrain_Old", "PlasticStrain_Trial");

  // 损伤场
  fm.addField(reg.createField("DoubleField", "Damage_Old", 1));
  fm.addField(reg.createField("DoubleField", "Damage_Trial", 1));
  fm.registerSwapPair("Damage_Old", "Damage_Trial");

  fm.addField(reg.createField("DoubleField", "VonMisesStress", 1));
}

void J2VoceLemaitreMat::bindStateVariables(PDCommon::Field::FieldManager &fm) {
  numParticles_ = fm.getFieldAs<double>("EqPlasticStrain_Old")->size();

  fieldEqPSOld_ = fm.getFieldAs<double>("EqPlasticStrain_Old");
  fieldEqPSTrial_ = fm.getFieldAs<double>("EqPlasticStrain_Trial");
  fieldPSOld_ = fm.getFieldAs<double>("PlasticStrain_Old");
  fieldPSTrial_ = fm.getFieldAs<double>("PlasticStrain_Trial");
  fieldDamageOld_ = fm.getFieldAs<double>("Damage_Old");
  fieldDamageTrial_ = fm.getFieldAs<double>("Damage_Trial");
  vonMises_ = fm.getFieldAs<double>("VonMisesStress")->dataPtr();

  eqPSOld_ = fieldEqPSOld_->dataPtr();
  eqPSTrial_ = fieldEqPSTrial_->dataPtr();
  pSOld_ = fieldPSOld_->dataPtr();
  pSTrial_ = fieldPSTrial_->dataPtr();
  damageOld_ = fieldDamageOld_->dataPtr();
  damageTrial_ = fieldDamageTrial_->dataPtr();
}

size_t J2VoceLemaitreMat::getNumStateVariables() const { return 0; }

void J2VoceLemaitreMat::evaluate() {
  // 不使用标准 evaluate，采用 ComputePK1Stress 显式调用
}

void J2VoceLemaitreMat::commitState() {
  if (!fieldEqPSOld_ || !fieldEqPSTrial_ || !fieldPSOld_ || !fieldPSTrial_)
    return;

  eqPSOld_ = fieldEqPSOld_->dataPtr();
  eqPSTrial_ = fieldEqPSTrial_->dataPtr();
  pSOld_ = fieldPSOld_->dataPtr();
  pSTrial_ = fieldPSTrial_->dataPtr();
  
  if (fieldDamageOld_ && fieldDamageTrial_) {
    damageOld_ = fieldDamageOld_->dataPtr();
    damageTrial_ = fieldDamageTrial_->dataPtr();
  }
}

Eigen::Matrix3d
J2VoceLemaitreMat::ComputeEngineeringStress(const Eigen::Matrix3d &strain) const {
  double e_trace = strain.trace();
  Eigen::Matrix3d stress =
      2.0 * mu_ * strain + lambda_ * e_trace * Eigen::Matrix3d::Identity();
  return stress;
}

Eigen::Matrix3d J2VoceLemaitreMat::ComputePK1Stress(const Eigen::Matrix3d &F,
                                                  int particleId, int stateMode) const {
  if (particleId < 0 || eqPSOld_ == nullptr) {
    Eigen::Matrix3d strain =
        0.5 * (F + F.transpose()) - Eigen::Matrix3d::Identity();
    return ComputeEngineeringStress(strain);
  }

  Eigen::Matrix3d I = Eigen::Matrix3d::Identity();
  Eigen::Matrix3d eps = Eigen::Matrix3d::Zero();

  if (largeDeformation_) {
    eps = 0.5 * (F.transpose() * F) - I;
  } else {
    eps = 0.5 * (F + F.transpose()) - I;
  }

  int idx9 = particleId * 9;

  if (stateMode == 1 || stateMode == 2) {
    // 冻结态本构预测
    Eigen::Matrix3d eps_p_fixed = Eigen::Matrix3d::Zero();
    const double* ps_ptr = (stateMode == 1) ? pSOld_ : pSTrial_;
    
    eps_p_fixed << ps_ptr[idx9], ps_ptr[idx9 + 1], ps_ptr[idx9 + 2],
        ps_ptr[idx9 + 3], ps_ptr[idx9 + 4], ps_ptr[idx9 + 5], ps_ptr[idx9 + 6],
        ps_ptr[idx9 + 7], ps_ptr[idx9 + 8];
    
    double damage_fixed = (stateMode == 1) ? damageOld_[particleId] : damageTrial_[particleId];

    Eigen::Matrix3d eps_e = eps - eps_p_fixed;
    Eigen::Matrix3d S_pred = ComputeEngineeringStress(eps_e); // 有效应力预测
    
    // 提取有效静水压以处理裂纹闭合
    double p_eff_fixed = S_pred.trace() / 3.0;
    Eigen::Matrix3d S_dev_pred = S_pred - p_eff_fixed * I;
    
    Eigen::Matrix3d nominal_S_pred = Eigen::Matrix3d::Zero();
    if (p_eff_fixed >= 0.0) {
      nominal_S_pred = (1.0 - damage_fixed) * S_pred; // 拉伸全折减
    } else {
      nominal_S_pred = (1.0 - damage_fixed) * S_dev_pred + p_eff_fixed * I; // 压缩闭合：静水压不折减
    }
    
    if (largeDeformation_) {
      return F * nominal_S_pred; // PK2 -> PK1
    }
    return nominal_S_pred;
  }

  // 正常本构计算 (stateMode == 0)
  Eigen::Matrix3d eps_p_old = Eigen::Matrix3d::Zero();
  eps_p_old << pSOld_[idx9], pSOld_[idx9 + 1], pSOld_[idx9 + 2],
      pSOld_[idx9 + 3], pSOld_[idx9 + 4], pSOld_[idx9 + 5], pSOld_[idx9 + 6],
      pSOld_[idx9 + 7], pSOld_[idx9 + 8];

  double alpha_old = eqPSOld_[particleId];
  double damage_old = damageOld_[particleId];

  // 试探有效弹性应变
  Eigen::Matrix3d eps_e_trial = eps - eps_p_old;
  double tr_eps_e = eps_e_trial.trace();

  // 偏试探有效应变 e
  Eigen::Matrix3d e_e_trial = eps_e_trial - (tr_eps_e / 3.0) * I;
  Eigen::Matrix3d S_trial = 2.0 * mu_ * e_e_trial; // 有效偏应力
  double hydro_stress = bulkModulus_ * tr_eps_e;   // 有效静水应力

  double norm_S = std::sqrt((S_trial.cwiseProduct(S_trial)).sum());
  double q_trial = std::sqrt(1.5) * norm_S;

  // 屈服判定 f = q_trial - sigma_y(alpha_old)
  double sigma_y_old = yieldStress_ + linearHardening_ * alpha_old + voceR_ * (1.0 - std::exp(-voceB_ * alpha_old));
  double f = q_trial - sigma_y_old;

  double dGamma = 0.0;
  Eigen::Matrix3d S_new = S_trial;
  double alpha_new = alpha_old;
  Eigen::Matrix3d eps_p_new = eps_p_old;

  if (f > 1e-8 * yieldStress_) {
    // 塑性流动：Newton-Raphson 求解 dGamma
    double H_prime_init = linearHardening_ + voceR_ * voceB_ * std::exp(-voceB_ * alpha_old);
    dGamma = f / (3.0 * mu_ + H_prime_init); // 初始猜测
    int iter = 0;
    while (iter < 20) {
      double sigma_y_current = yieldStress_ + linearHardening_ * (alpha_old + dGamma) + voceR_ * (1.0 - std::exp(-voceB_ * (alpha_old + dGamma)));
      double g = q_trial - 3.0 * mu_ * dGamma - sigma_y_current;
      if (std::abs(g) < 1e-6 * yieldStress_) break;
      double dg = -3.0 * mu_ - (linearHardening_ + voceR_ * voceB_ * std::exp(-voceB_ * (alpha_old + dGamma)));
      dGamma -= g / dg;
      iter++;
    }

    Eigen::Matrix3d n_dir = Eigen::Matrix3d::Zero();
    if (norm_S > 1e-16) {
      n_dir = S_trial / norm_S;
    }
    Eigen::Matrix3d dEps_p = dGamma * std::sqrt(1.5) * n_dir;
    eps_p_new += dEps_p;
    alpha_new += dGamma;
    S_new = S_trial - 2.0 * mu_ * dEps_p;
  }

  // 损伤演化
  double damage_new = damage_old;
  if (alpha_new > damageThreshold_ && dGamma > 0.0) {
    // 有效等效应力 q_new
    double q_new = q_trial - 3.0 * mu_ * dGamma;
    
    // Y 的计算根据裂纹闭合效应 (Unilateral Effect) 分支
    double Y = 0.0;
    if (hydro_stress >= 0.0) {
      // 受拉状态：静水压和偏应力都贡献损伤 (注意这里 hydro_stress 已是有效应力，故无需除以 W^2)
      Y = (q_new * q_new) / (6.0 * mu_) + (hydro_stress * hydro_stress) / (2.0 * bulkModulus_);
    } else {
      // 受压状态：仅偏应力贡献损伤（裂纹闭合，静水压不作恶）
      Y = (q_new * q_new) / (6.0 * mu_);
    }

    // 损伤加速
    if (damage_old >= damageAccelThreshold_) {
      Y *= damageAccelFactor_;
    }
    
    // 求解 W_{n+1} 的非线性方程： W - W_n + (Y/S)^s * dGamma / W = 0
    // (采用隐式计算，方程中包含当前步未知的 W)
    if (Y > 0.0) {
      double Wn = 1.0 - damage_old;
      double W = Wn;
      double f2 = Y / lemaitreS_;
      double Y_term = std::pow(f2, lemaitre_s_) * dGamma;
      
      int it = 0;
      while (it < 200) {
        double res2 = W - Wn + Y_term / W;
        if (std::abs(res2) < 1e-8) break;
        double H22 = 1.0 - Y_term / (W * W);
        if (std::abs(H22) < 1e-12) break; // 防止奇异
        W -= res2 / H22;
        it++;
      }
      
      if (W < 0.001) W = 0.001; // 防止完全破断导致的数值奇异
      damage_new = 1.0 - W;
    }
  }
  
  if (damage_new > criticalDamage_) {
    damage_new = criticalDamage_;
  }

  // 计算名义应力 (用最新的 D_{n+1} 进行折减)
  Eigen::Matrix3d sigma_nom = Eigen::Matrix3d::Zero();
  if (hydro_stress >= 0.0) {
    // 受拉：偏应力与静水力全盘折减
    Eigen::Matrix3d sigma_eff = S_new + hydro_stress * I;
    sigma_nom = (1.0 - damage_new) * sigma_eff;
  } else {
    // 受压（裂纹闭合）：仅偏应力折减，静水力不折减！
    sigma_nom = (1.0 - damage_new) * S_new + hydro_stress * I;
  }

  // 记录到 Trial 缓冲
  eqPSTrial_[particleId] = alpha_new;
  pSTrial_[idx9] = eps_p_new(0, 0);
  pSTrial_[idx9 + 1] = eps_p_new(0, 1);
  pSTrial_[idx9 + 2] = eps_p_new(0, 2);
  pSTrial_[idx9 + 3] = eps_p_new(1, 0);
  pSTrial_[idx9 + 4] = eps_p_new(1, 1);
  pSTrial_[idx9 + 5] = eps_p_new(1, 2);
  pSTrial_[idx9 + 6] = eps_p_new(2, 0);
  pSTrial_[idx9 + 7] = eps_p_new(2, 1);
  pSTrial_[idx9 + 8] = eps_p_new(2, 2);

  damageTrial_[particleId] = damage_new;

  // 辅助输出
  double q_nom = (1.0 - damage_new) * (q_trial - 3.0 * mu_ * dGamma);
  vonMises_[particleId] = q_nom;

  Eigen::Matrix3d P1 = sigma_nom;
  if (largeDeformation_) {
    P1 = F * sigma_nom;
  }

  return P1;
}

} // namespace PDCommon::Material
