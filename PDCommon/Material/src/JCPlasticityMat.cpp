#include "JCPlasticityMat.h"
#include "FieldManager.h"
#include "FieldRegistry.h"
#include "Logger.h"
#include "MaterialRegistry.h"
#include <algorithm>
#include <cmath>

namespace PDCommon::Material {

REGISTER_MATERIAL_TYPE(JCPlasticityMat, [](const std::string &name) {
  return std::make_unique<PDCommon::Material::JCPlasticityMat>(name);
});

JCPlasticityMat::JCPlasticityMat(const std::string &name)
    : J2PlasticityMat(name) {}

void JCPlasticityMat::initialize(const YAML::Node &matNode) {
  // 此时基类的 initialize 也会一并读取 J2 相关的 YieldStress, HardeningModulus
  // 等
  J2PlasticityMat::initialize(matNode);

  if (matNode["JC_D1"]) {
    D1_ = matNode["JC_D1"].as<double>();
  }
  if (matNode["JC_D2"]) {
    D2_ = matNode["JC_D2"].as<double>();
  }
  if (matNode["JC_D3"]) {
    D3_ = matNode["JC_D3"].as<double>();
  }

  useTriaxiality_ = (std::abs(D2_) > 1e-30 || std::abs(D3_) > 1e-30);

  LOG_INFO(
      "[JCPlasticityMat] Initialized JC Damage parameters: D1=" +
      std::to_string(D1_) + " D2=" + std::to_string(D2_) +
      " D3=" + std::to_string(D3_) +
      (useTriaxiality_ ? " [Triaxiality Active]" : " [Constant Threshold]"));
}

void JCPlasticityMat::allocateStateVariables(
    PDCommon::Field::FieldManager &fm) {
  // 调用基类分配 J2 模型相关的状态变量
  J2PlasticityMat::allocateStateVariables(fm);

  // 增加统一的损伤变量场
  auto &reg = PDCommon::Field::FieldRegistry::getInstance();
  fm.addField(reg.createField("DoubleField", "Damage_Old", 1));
  fm.addField(reg.createField("DoubleField", "Damage_Trial", 1));

  // 路线1核心：向中央注册互相绑定的情侣关系，交接 Swap 执行权
  fm.registerSwapPair("Damage_Old", "Damage_Trial");
}

void JCPlasticityMat::bindStateVariables(PDCommon::Field::FieldManager &fm) {
  J2PlasticityMat::bindStateVariables(fm);

  fieldDamageOld_ = fm.getFieldAs<double>("Damage_Old");
  fieldDamageTrial_ = fm.getFieldAs<double>("Damage_Trial");

  damageOld_ = fieldDamageOld_->dataPtr();
  damageTrial_ = fieldDamageTrial_->dataPtr();
}

void JCPlasticityMat::commitState() {
  J2PlasticityMat::commitState();

  if (!fieldDamageOld_ || !fieldDamageTrial_)
    return;

  // 物理内存已被中央 FieldManager O(1) Swap 互换，此处只需刷新原生指针缓存
  // 彻底免疫多材料踩踏 Bug！
  damageOld_ = fieldDamageOld_->dataPtr();
  damageTrial_ = fieldDamageTrial_->dataPtr();
}

Eigen::Matrix3d JCPlasticityMat::ComputePK1Stress(const Eigen::Matrix3d &F,
                                                  int particleId) const {
  // 先通过基类求得应力和 J2 塑性应变 Trial 态
  Eigen::Matrix3d stress = J2PlasticityMat::ComputePK1Stress(F, particleId);

  // 如果无状态变量指针或是负的ID (纯弹性回调)，则直接返回应力，不计算损伤
  if (particleId < 0 || eqPSOld_ == nullptr || damageOld_ == nullptr) {
    return stress;
  }

  // 基类已经把当步塑性计算完毕，并填入了 eqPSTrial_[particleId]
  double alpha_old = eqPSOld_[particleId];
  double alpha_new = eqPSTrial_[particleId];
  double delta_eps_p = std::max(0.0, alpha_new - alpha_old);

  // 追踪静水压 (小变形时 PK1 对角线和依然接近 Cauchy 的迹)
  double sigma_m = stress.trace() / 3.0;
  double sigma_eq = vonMises_[particleId];
  // 强制拉伸损伤判定：仅在拉伸状态（静水压 > 0）下才允许损伤累积
  // if (sigma_m / sigma_eq <= -0.333333333) {
  //   delta_eps_p = 0.0;
  // }

  double eps_f = D1_;
  if (useTriaxiality_) {
    double vm = vonMises_[particleId]; // J2 刚算出来的 Mises 应力
    double eta = sigma_m / std::max(vm, 1e-6);
    eps_f = D1_ + D2_ * std::exp(D3_ * eta);
    if (eps_f < 0.0)
      eps_f = 0.0;
  }

  double dD = (eps_f > 1e-30) ? (delta_eps_p / eps_f) : 0.0;

  double newDamage = damageOld_[particleId] + dD;
  if (newDamage >= 1.0)
    newDamage = 1.0;

  damageTrial_[particleId] = newDamage;

  return stress;
}

} // namespace PDCommon::Material
