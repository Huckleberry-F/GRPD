#include "J2PlasticityMat.h"
#include "FieldManager.h"
#include "FieldRegistry.h"
#include "Logger.h"
#include "MaterialRegistry.h"

namespace PDCommon::Material {

// ---------------------------------------------------------------------------
// 向工厂注册材料类�?
// ---------------------------------------------------------------------------
REGISTER_MATERIAL_TYPE(J2PlasticityMat, [](const std::string &name) {
  return std::make_unique<PDCommon::Material::J2PlasticityMat>(name);
});

J2PlasticityMat::J2PlasticityMat(const std::string &name)
    : MechanicalMaterial(name) {}

void J2PlasticityMat::initialize(const YAML::Node &matNode) {
  MechanicalMaterial::initialize(matNode);

  if (matNode["Density"])
    density_ = matNode["Density"].as<double>();
  if (matNode["YoungsModulus"])
    youngsModulus_ = matNode["YoungsModulus"].as<double>();
  if (matNode["PoissonsRatio"])
    poissonsRatio_ = matNode["PoissonsRatio"].as<double>();

  if (matNode["YieldStress"])
    yieldStress_ = matNode["YieldStress"].as<double>();
  if (matNode["HardeningModulus"])
    hardeningModulus_ = matNode["HardeningModulus"].as<double>();

  if (matNode["LargeDeformation"])
    largeDeformation_ = matNode["LargeDeformation"].as<bool>();

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

  if (density_ <= 0.0 || youngsModulus_ <= 0.0 || poissonsRatio_ < 0.0 ||
      poissonsRatio_ > 0.5) {
    LOG_ERROR(
        "[J2PlasticityMat] Invalid basic elastic parameters for material: " +
        name_);
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

void J2PlasticityMat::allocateStateVariables(
    PDCommon::Field::FieldManager &fm) {
  // 获取当前最大的粒子总数（因为所有的场最终都�?SOA 平铺数组�?
  // 虽然材料�?allocation 通常发生在具体粒子分配之前，但是 FieldManager
  // 注册是依靠类型名�?
  auto &reg = PDCommon::Field::FieldRegistry::getInstance();

  fm.addField(reg.createField("DoubleField", "EqPlasticStrain_Old", 1));
  fm.addField(reg.createField("DoubleField", "EqPlasticStrain_Trial", 1));
  fm.registerSwapPair("EqPlasticStrain_Old", "EqPlasticStrain_Trial");

  fm.addField(reg.createField("DoubleField", "PlasticStrain_Old", 9));
  fm.addField(reg.createField("DoubleField", "PlasticStrain_Trial", 9));
  fm.registerSwapPair("PlasticStrain_Old", "PlasticStrain_Trial");

  fm.addField(reg.createField("DoubleField", "VonMisesStress", 1));

  if (hardeningType_ == HardeningType::Kinematic ||
      hardeningType_ == HardeningType::Combined) {
    fm.addField(reg.createField("DoubleField", "BackStress_Old", 9));
    fm.addField(reg.createField("DoubleField", "BackStress_Trial", 9));
    fm.registerSwapPair("BackStress_Old", "BackStress_Trial");
  }

  // 注意：真实分配是在所有类型申请完后统一进行的，因此�?allocate
  // 阶段无法取回裸指针�?
}

void J2PlasticityMat::bindStateVariables(PDCommon::Field::FieldManager &fm) {
  numParticles_ = fm.getFieldAs<double>("EqPlasticStrain_Old")->size();

  fieldEqPSOld_ = fm.getFieldAs<double>("EqPlasticStrain_Old");
  fieldEqPSTrial_ = fm.getFieldAs<double>("EqPlasticStrain_Trial");
  fieldPSOld_ = fm.getFieldAs<double>("PlasticStrain_Old");
  fieldPSTrial_ = fm.getFieldAs<double>("PlasticStrain_Trial");
  vonMises_ = fm.getFieldAs<double>("VonMisesStress")->dataPtr();

  eqPSOld_ = fieldEqPSOld_->dataPtr();
  eqPSTrial_ = fieldEqPSTrial_->dataPtr();
  pSOld_ = fieldPSOld_->dataPtr();
  pSTrial_ = fieldPSTrial_->dataPtr();

  if (hardeningType_ == HardeningType::Kinematic ||
      hardeningType_ == HardeningType::Combined) {

    fieldBetaOld_ = fm.getFieldAs<double>("BackStress_Old");
    fieldBetaTrial_ = fm.getFieldAs<double>("BackStress_Trial");

    betaOld_ = fieldBetaOld_->dataPtr();
    betaTrial_ = fieldBetaTrial_->dataPtr();
  }
}

size_t J2PlasticityMat::getNumStateVariables() const { return 0; }

void J2PlasticityMat::evaluate() {
  // 不使用标�?evaluate，采�?ComputePK1Stress 显式调用
}

void J2PlasticityMat::commitState() {
  if (!fieldEqPSOld_ || !fieldEqPSTrial_ || !fieldPSOld_ || !fieldPSTrial_)
    return;

  // 物理内存的实质翻转已�?CentralDifference 回合末调�?fm.executeAllRegisteredSwaps() 统一�?O(1) 内完�?
  // 此处材料仅需执行指针的同步刷新，彻底解耦物理计算与内存排布
  eqPSOld_ = fieldEqPSOld_->dataPtr();
  eqPSTrial_ = fieldEqPSTrial_->dataPtr();
  pSOld_ = fieldPSOld_->dataPtr();
  pSTrial_ = fieldPSTrial_->dataPtr();

  if (betaOld_ && betaTrial_ && fieldBetaOld_ && fieldBetaTrial_) {
    betaOld_ = fieldBetaOld_->dataPtr();
    betaTrial_ = fieldBetaTrial_->dataPtr();
  }
}

// ---------------------------------------------------------------------------
// 纯无状态辅助函数：计算工程应力
// ---------------------------------------------------------------------------
Eigen::Matrix3d
J2PlasticityMat::ComputeEngineeringStress(const Eigen::Matrix3d &strain) const {
  // 这里属于纯线性胡克定律回退
  double e_trace = strain.trace();
  Eigen::Matrix3d stress =
      2.0 * mu_ * strain + lambda_ * e_trace * Eigen::Matrix3d::Identity();
  return stress;
}

// ---------------------------------------------------------------------------
// 核心状态相关本构算法：包含小变形下的径向返�?
// ---------------------------------------------------------------------------
Eigen::Matrix3d J2PlasticityMat::ComputePK1Stress(const Eigen::Matrix3d &F,
                                                  int particleId, int stateMode) const {
  if (particleId < 0 || eqPSOld_ == nullptr) {
    // 降级为纯弹�?
    Eigen::Matrix3d strain =
        0.5 * (F + F.transpose()) - Eigen::Matrix3d::Identity();
    return ComputeEngineeringStress(strain);
  }

  Eigen::Matrix3d I = Eigen::Matrix3d::Identity();
  Eigen::Matrix3d eps = Eigen::Matrix3d::Zero();

  if (largeDeformation_) {
    // [大变形修复]：采�?Green-Lagrange 应变 E = 0.5 * (F^T * F - I)
    // 此公式计算极快（无需极分解），且具有绝对的旋转客观性！
    // 能够彻底消除在巨大应力（大硬化模量）下，裂纹尖端因刚体大转动引发的虚假畸变应变！
    eps = 0.5 * (F.transpose() * F - I);
  } else {
    // 小变形情况（近似非线性截断）
    eps = 0.5 * (F + F.transpose()) - I;
  }

  int idx9 = particleId * 9;

  if (stateMode == 1 || stateMode == 2) {
    // [ADR 初始刚度法核心逻辑]
    // stateMode == 1: outerIter=0的冻结态，读取Old�?上一个载荷子步的塑性历�?
    // stateMode == 2: outerIter>0的冻结态，读取Trial�?上一次外循环本构试探更新后的最新塑性场)
    Eigen::Matrix3d eps_p_fixed = Eigen::Matrix3d::Zero();
    const double* ps_ptr = (stateMode == 1) ? pSOld_ : pSTrial_;

    eps_p_fixed << ps_ptr[idx9], ps_ptr[idx9 + 1], ps_ptr[idx9 + 2],
        ps_ptr[idx9 + 3], ps_ptr[idx9 + 4], ps_ptr[idx9 + 5], ps_ptr[idx9 + 6],
        ps_ptr[idx9 + 7], ps_ptr[idx9 + 8];

    Eigen::Matrix3d eps_e = eps - eps_p_fixed;

    // [智能标量切线预测�?(Scalar Tangent Predictor)]
    // �?stateMode == 2 且当前粒子发生过塑性流动时，大幅削弱剪切刚度，以近似一致切线刚度矩阵的效果，极大地加速外�?NR 迭代�?
    double current_mu = mu_;
    double current_lambda = lambda_;

    if (stateMode == 2) {
       // 如果 Trial 塑性应变大�?Old 塑性应变，说明上一�?NR 迭代步发生了活跃的塑性屈�?
       if (eqPSTrial_[particleId] > eqPSOld_[particleId] + 1.0e-12) {
           double H = hardeningModulus_;
           // 剪切模量折减系数 (基于 1D 弹塑性切线近�?
           double beta = H / (H + 3.0 * mu_);
           // 为了防止刚度矩阵奇异导致内循环除零或发散，保留至�?5% 的剪切刚�?
           beta = std::max(beta, 0.05);

           current_mu = mu_ * beta;
           // 为了保持体积刚度 (Bulk Modulus) K 不变（塑性是纯偏斜的，不影响体积变形），反推等效�?lambda
           double K_bulk = lambda_ + (2.0 / 3.0) * mu_;
           current_lambda = K_bulk - (2.0 / 3.0) * current_mu;
       }
    }

    double e_trace = eps_e.trace();
    Eigen::Matrix3d S_pred = 2.0 * current_mu * eps_e + current_lambda * e_trace * I;

    if (largeDeformation_) {
      return F * S_pred; // �?PK2 应力推回 PK1 应力
    }
    return S_pred; // 直接返回刚度预测，不更新塑性参�?
  }

  // 以下为正常本构计�?(stateMode == 0)
  Eigen::Matrix3d eps_p_old = Eigen::Matrix3d::Zero();
  eps_p_old << pSOld_[idx9], pSOld_[idx9 + 1], pSOld_[idx9 + 2],
      pSOld_[idx9 + 3], pSOld_[idx9 + 4], pSOld_[idx9 + 5], pSOld_[idx9 + 6],
      pSOld_[idx9 + 7], pSOld_[idx9 + 8];

  double alpha_old = eqPSOld_[particleId];

  // 试探弹性应�?
  Eigen::Matrix3d eps_e_trial = eps - eps_p_old;
  double tr_eps_e = eps_e_trial.trace();

  // 偏试探应�?e
  Eigen::Matrix3d e_e_trial = eps_e_trial - (tr_eps_e / 3.0) * I;

  // 试探偏应�?S
  Eigen::Matrix3d S_trial = 2.0 * mu_ * e_e_trial;

  double hydro_stress = bulkModulus_ * tr_eps_e;

  // Von Mises 等效应力
  double norm_S = std::sqrt((S_trial.cwiseProduct(S_trial)).sum());
  double q_trial = std::sqrt(1.5) * norm_S;

  // 当前屈服�?
  double R = yieldStress_ + hardeningModulus_ * alpha_old;
  double f = q_trial - R;

  // --- 方案 C 性能优化：无分支向量�?(Branchless Vectorization) ---
  // 彻底抹除导致 AVX/SSE 崩溃退化的 if-else
  bool isYielding = (f > 1e-8 * yieldStress_);
  double dGamma = isYielding ? (f / (3.0 * mu_ + hardeningModulus_)) : 0.0;

  Eigen::Matrix3d n_dir = Eigen::Matrix3d::Zero();
  if (norm_S > 1e-16) {
    n_dir = S_trial / norm_S;
  }

  Eigen::Matrix3d dEps_p = dGamma * std::sqrt(1.5) * n_dir;
  Eigen::Matrix3d eps_p_new = eps_p_old + dEps_p;
  double alpha_new = alpha_old + dGamma;

  Eigen::Matrix3d S_new = S_trial - 2.0 * mu_ * dEps_p;
  Eigen::Matrix3d sigma = S_new + hydro_stress * I;

  // 记录�?Trial 缓冲
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

  // 输出 Von Mises 应力 (这也是无分支)
  vonMises_[particleId] =
      isYielding ? (yieldStress_ + hardeningModulus_ * alpha_new) : q_trial;

  // 第一�?P-K 应力在大转动下应满足 P = F * S�?
  Eigen::Matrix3d P1 = sigma;
  if (largeDeformation_) {
    // 此时�?sigma 实际上是在初始构型下的第二类 P-K 应力 (PK2)�?
    // 必须将其推前 (Push-forward) 为第一�?P-K 应力 (PK1) 返回给引擎�?
    P1 = F * sigma;
  }

  return P1;
}

} // namespace PDCommon::Material
