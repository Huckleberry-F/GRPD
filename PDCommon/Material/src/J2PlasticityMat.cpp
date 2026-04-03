#include "J2PlasticityMat.h"
#include "FieldManager.h"
#include "FieldRegistry.h"
#include "Logger.h"
#include "MaterialRegistry.h"

namespace PDCommon::Material {

// ---------------------------------------------------------------------------
// 向工厂注册材料类型
// ---------------------------------------------------------------------------
REGISTER_MATERIAL_TYPE(J2PlasticityMat, [](const std::string &name) {
  return std::make_unique<PDCommon::Material::J2PlasticityMat>(name);
});

J2PlasticityMat::J2PlasticityMat(const std::string &name) : MechanicalMaterial(name) {}

void J2PlasticityMat::initialize(const YAML::Node &matNode) {
  MechanicalMaterial::initialize(matNode);

  if (matNode["Density"]) density_ = matNode["Density"].as<double>();
  if (matNode["YoungsModulus"]) youngsModulus_ = matNode["YoungsModulus"].as<double>();
  if (matNode["PoissonsRatio"]) poissonsRatio_ = matNode["PoissonsRatio"].as<double>();

  if (matNode["YieldStress"]) yieldStress_ = matNode["YieldStress"].as<double>();
  if (matNode["HardeningModulus"]) hardeningModulus_ = matNode["HardeningModulus"].as<double>();
  
  if (matNode["LargeDeformation"]) largeDeformation_ = matNode["LargeDeformation"].as<bool>();

  if (matNode["HardeningType"]) {
    std::string type = matNode["HardeningType"].as<std::string>();
    if (type == "Kinematic") {
      hardeningType_ = HardeningType::Kinematic;
      LOG_INFO("[J2PlasticityMat] Hardening Type: Kinematic");
    } else if (type == "Combined") {
      hardeningType_ = HardeningType::Combined;
      LOG_INFO("[J2PlasticityMat] Hardening Type: Combined");
    } else {
      hardeningType_ = HardeningType::Isotropic;
      LOG_INFO("[J2PlasticityMat] Hardening Type: Isotropic");
    }
  }

  if (density_ <= 0.0 || youngsModulus_ <= 0.0 || poissonsRatio_ < 0.0 || poissonsRatio_ > 0.5) {
    LOG_ERROR("[J2PlasticityMat] Invalid basic elastic parameters for material: " + name_);
    exit(EXIT_FAILURE);
  }

  computeLameParameters();
}

void J2PlasticityMat::computeLameParameters() {
  lambda_ = (youngsModulus_ * poissonsRatio_) /
            ((1.0 + poissonsRatio_) * (1.0 - 2.0 * poissonsRatio_));
  mu_ = youngsModulus_ / (2.0 * (1.0 + poissonsRatio_));
  bulkModulus_ = youngsModulus_ / (3.0 * (1.0 - 2.0 * poissonsRatio_));
}

void J2PlasticityMat::allocateStateVariables(PDCommon::Field::FieldManager &fm) {
  // 获取当前最大的粒子总数（因为所有的场最终都是 SOA 平铺数组）
  // 虽然材料的 allocation 通常发生在具体粒子分配之前，但是 FieldManager 注册是依靠类型名的
  auto &reg = PDCommon::Field::FieldRegistry::getInstance();

  fm.addField(reg.createField("DoubleField", "EqPlasticStrain_Old", 1));
  fm.addField(reg.createField("DoubleField", "EqPlasticStrain_Trial", 1));
  fm.addField(reg.createField("DoubleField", "PlasticStrain_Old", 9));
  fm.addField(reg.createField("DoubleField", "PlasticStrain_Trial", 9));

  if (hardeningType_ == HardeningType::Kinematic || hardeningType_ == HardeningType::Combined) {
    fm.addField(reg.createField("DoubleField", "BackStress_Old", 9));
    fm.addField(reg.createField("DoubleField", "BackStress_Trial", 9));
  }

  // 注意：真实分配是在所有类型申请完后统一进行的，因此在 allocate 阶段无法取回裸指针。
}

void J2PlasticityMat::bindStateVariables(PDCommon::Field::FieldManager &fm) {
  numParticles_ = fm.getFieldAs<double>("EqPlasticStrain_Old")->size();

  eqPSOld_   = fm.getFieldAs<double>("EqPlasticStrain_Old")->dataPtr();
  eqPSTrial_ = fm.getFieldAs<double>("EqPlasticStrain_Trial")->dataPtr();
  pSOld_     = fm.getFieldAs<double>("PlasticStrain_Old")->dataPtr();
  pSTrial_   = fm.getFieldAs<double>("PlasticStrain_Trial")->dataPtr();

  if (hardeningType_ == HardeningType::Kinematic || hardeningType_ == HardeningType::Combined) {
    betaOld_   = fm.getFieldAs<double>("BackStress_Old")->dataPtr();
    betaTrial_ = fm.getFieldAs<double>("BackStress_Trial")->dataPtr();
  }
}

size_t J2PlasticityMat::getNumStateVariables() const { return 0; }

void J2PlasticityMat::evaluate() {
  // 不使用标准 evaluate，采用 ComputePK1Stress 显式调用
}

void J2PlasticityMat::commitState() {
  // 如果指针为空，放弃写入
  if (!eqPSOld_ || !eqPSTrial_ || !pSOld_ || !pSTrial_) return;

  // 在 ADR 子步收敛时，推进状态 (Trial -> Old)
  for (size_t i = 0; i < numParticles_; ++i) {
    eqPSOld_[i] = eqPSTrial_[i];
    
    int idx9 = i * 9;
    for (int j = 0; j < 9; ++j) {
      pSOld_[idx9 + j] = pSTrial_[idx9 + j];
    }

    if (betaOld_ && betaTrial_) {
      for (int j = 0; j < 9; ++j) {
        betaOld_[idx9 + j] = betaTrial_[idx9 + j];
      }
    }
  }
}

// ---------------------------------------------------------------------------
// 纯无状态辅助函数：计算工程应力
// ---------------------------------------------------------------------------
Eigen::Matrix3d J2PlasticityMat::ComputeEngineeringStress(const Eigen::Matrix3d &strain) const {
  // 这里属于纯线性胡克定律回退
  double e_trace = strain.trace();
  Eigen::Matrix3d stress = 2.0 * mu_ * strain + lambda_ * e_trace * Eigen::Matrix3d::Identity();
  return stress;
}

// ---------------------------------------------------------------------------
// 核心状态相关本构算法：包含小变形下的径向返回
// ---------------------------------------------------------------------------
Eigen::Matrix3d J2PlasticityMat::ComputePK1Stress(const Eigen::Matrix3d &F, int particleId) const {
  if (particleId < 0 || eqPSOld_ == nullptr) {
    // 降级为纯弹性
    Eigen::Matrix3d strain = 0.5 * (F + F.transpose()) - Eigen::Matrix3d::Identity();
    return ComputeEngineeringStress(strain);
  }

  Eigen::Matrix3d I = Eigen::Matrix3d::Identity();
  Eigen::Matrix3d eps = Eigen::Matrix3d::Zero();

  if (largeDeformation_) {
    // 预留大变形极分解
    // F = R * U => eps = U - I
    // 这里因为特征值分解太耗时，暂退回小变形
    eps = 0.5 * (F + F.transpose()) - I;
  } else {
    // 小变形情况（近似非线性截断）
    eps = 0.5 * (F + F.transpose()) - I;
  }

  // 读取 Old 塑性应变
  Eigen::Matrix3d eps_p_old = Eigen::Matrix3d::Zero();
  int idx9 = particleId * 9;
  eps_p_old << pSOld_[idx9],     pSOld_[idx9 + 1], pSOld_[idx9 + 2],
               pSOld_[idx9 + 3], pSOld_[idx9 + 4], pSOld_[idx9 + 5],
               pSOld_[idx9 + 6], pSOld_[idx9 + 7], pSOld_[idx9 + 8];

  double alpha_old = eqPSOld_[particleId];

  // 试探弹性应变
  Eigen::Matrix3d eps_e_trial = eps - eps_p_old;
  double tr_eps_e = eps_e_trial.trace();
  
  // 偏试探应变 e
  Eigen::Matrix3d e_e_trial = eps_e_trial - (tr_eps_e / 3.0) * I;
  
  // 试探偏应力 S
  Eigen::Matrix3d S_trial = 2.0 * mu_ * e_e_trial;
  
  double hydro_stress = bulkModulus_ * tr_eps_e;

  // Von Mises 等效应力
  double norm_S = std::sqrt((S_trial.cwiseProduct(S_trial)).sum());
  double q_trial = std::sqrt(1.5) * norm_S;

  // 当前屈服面
  double R = yieldStress_ + hardeningModulus_ * alpha_old;
  double f = q_trial - R;

  Eigen::Matrix3d sigma = Eigen::Matrix3d::Zero();
  Eigen::Matrix3d eps_p_new = eps_p_old;
  double alpha_new = alpha_old;

  if (f <= 1e-8 * yieldStress_) { // 弹性卸载/加载
    sigma = S_trial + hydro_stress * I;
  } else { // 塑性屈服 (Radial Return)
    double dGamma = f / (3.0 * mu_ + hardeningModulus_);
    Eigen::Matrix3d n_dir = Eigen::Matrix3d::Zero();
    if (norm_S > 1e-16) {
      n_dir = S_trial / norm_S;
    }
    Eigen::Matrix3d dEps_p = dGamma * std::sqrt(1.5) * n_dir;
    
    eps_p_new = eps_p_old + dEps_p;
    
    Eigen::Matrix3d S_new = S_trial - 2.0 * mu_ * dEps_p;
    sigma = S_new + hydro_stress * I;
  }

  // 记录到 Trial 缓冲 (由 ADR 独立判断收敛后才落盘到 Old)
  eqPSTrial_[particleId] = alpha_new;
  pSTrial_[idx9]     = eps_p_new(0, 0); pSTrial_[idx9 + 1] = eps_p_new(0, 1); pSTrial_[idx9 + 2] = eps_p_new(0, 2);
  pSTrial_[idx9 + 3] = eps_p_new(1, 0); pSTrial_[idx9 + 4] = eps_p_new(1, 1); pSTrial_[idx9 + 5] = eps_p_new(1, 2);
  pSTrial_[idx9 + 6] = eps_p_new(2, 0); pSTrial_[idx9 + 7] = eps_p_new(2, 1); pSTrial_[idx9 + 8] = eps_p_new(2, 2);

  // 第一类 P-K 应力在大转动下应满足 P = F * S。但在古典小应变下 P ≈ sigma
  Eigen::Matrix3d P1 = sigma;
  if (largeDeformation_) {
    // TODO: Large deformation PK1 stress correct mapped from unrotated Cauchy
  }
  
  return P1;
}

} // namespace PDCommon::Material
