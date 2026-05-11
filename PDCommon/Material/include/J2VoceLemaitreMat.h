#ifndef PDCOMMON_MATERIAL_J2_VOCE_LEMAITRE_MAT_H
#define PDCOMMON_MATERIAL_J2_VOCE_LEMAITRE_MAT_H

// ============================================================================
// J2VoceLemaitreMat.h - J2 弹塑性本构结合 Voce 硬化与 Lemaitre 损伤
//
// 职责：
//   实现基于 Voce 等向强化律和 Lemaitre 损伤演化律的弹塑性损伤全耦合本构。
//   在有效应力空间采用径向返回算法进行积分，利用应变等效原理实现塑性求解与
//   损伤演化的完美解耦。
//   本构计算后的损伤状态将同步输出到系统的 Damage_Trial 场，直接驱动断裂机制。
// ============================================================================

#include "MechanicalMaterial.h"
#include "TypedField.h"
#include <string>
#include <yaml-cpp/yaml.h>

namespace PDCommon::Material {

class J2VoceLemaitreMat : public MechanicalMaterial {
public:
  explicit J2VoceLemaitreMat(const std::string &name = "");
  ~J2VoceLemaitreMat() override = default;

  // -----------------------------------------------------------------------
  // 实现 MechanicalMaterial 纯虚接口
  // -----------------------------------------------------------------------
  Eigen::Matrix3d
  ComputeEngineeringStress(const Eigen::Matrix3d &strain) const override;

  Eigen::Matrix3d ComputePK1Stress(const Eigen::Matrix3d &F, int particleId = -1, int stateMode = 0) const override;

  // -----------------------------------------------------------------------
  // 实现 Material 基类虚函数
  // -----------------------------------------------------------------------
  void initialize(const YAML::Node &matNode) override;
  void allocateStateVariables(PDCommon::Field::FieldManager &fm) override;
  void bindStateVariables(PDCommon::Field::FieldManager &fm) override;
  size_t getNumStateVariables() const override;
  void evaluate() override;
  void commitState() override; // 将 Trial 场推进为 Old 场

  double getDensity() const override { return density_; }
  double getYoungsModulus() const override { return youngsModulus_; }
  double getPoissonsRatio() const override { return poissonsRatio_; }

protected:
  // 基本物理属性
  double density_{-1.0};
  double youngsModulus_{-1.0};
  double poissonsRatio_{-1.0};

  // 派生拉梅常数
  double lambda_{0.0};
  double mu_{0.0};
  double bulkModulus_{0.0};

  // 塑性属性 (Linear+Voce Hardening: sigma_y = sigma_0 + K1*p + R_inf * (1 - exp(-b*p)))
  double yieldStress_{1e20}; // sigma_0
  double linearHardening_{0.0}; // K1
  double voceR_{0.0};        // R_inf
  double voceB_{0.0};        // b
  bool largeDeformation_{false}; // 是否使用大变形极分解

  // 损伤属性 (Lemaitre)
  double lemaitreS_{1.0};      // S
  double lemaitre_s_{1.0};     // s
  double damageThreshold_{0.0}; // p_D
  double criticalDamage_{1.0};  // D_c
  double damageAccelThreshold_{0.4}; // K4: 触发加速的损伤阈值
  double damageAccelFactor_{10.0};   // 达到阈值后的 Y 放大倍数

  // 算法缓存：缓存所有材料辖区下的粒子数
  size_t numParticles_{0};

  // 状态变量裸指针 (SoA加速读取)
  // 1. 等效塑性应变 (EqPlasticStrain, 维数 1)
  double *eqPSOld_{nullptr};
  double *eqPSTrial_{nullptr};

  // 2. 塑性应变张量 (PlasticStrain, 维数 9)
  double *pSOld_{nullptr};
  double *pSTrial_{nullptr};

  // 3. 损伤变量 (Damage, 维数 1) - 直接利用 Damage_Trial 和 Damage_Old
  double *damageOld_{nullptr};
  double *damageTrial_{nullptr};

  // 4. 辅助推导输出：Von Mises 输出标量
  double *vonMises_{nullptr};

  // 面向 O(1) 交换的 TypedField 原生指针句柄缓存
  PDCommon::Field::TypedField<double> *fieldEqPSOld_{nullptr};
  PDCommon::Field::TypedField<double> *fieldEqPSTrial_{nullptr};
  PDCommon::Field::TypedField<double> *fieldPSOld_{nullptr};
  PDCommon::Field::TypedField<double> *fieldPSTrial_{nullptr};
  PDCommon::Field::TypedField<double> *fieldDamageOld_{nullptr};
  PDCommon::Field::TypedField<double> *fieldDamageTrial_{nullptr};

  void computeLameParameters();
};

} // namespace PDCommon::Material

#endif // PDCOMMON_MATERIAL_J2_VOCE_LEMAITRE_MAT_H
