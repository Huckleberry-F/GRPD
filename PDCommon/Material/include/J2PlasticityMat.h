#ifndef PDCOMMON_MATERIAL_J2_PLASTICITY_MAT_H
#define PDCOMMON_MATERIAL_J2_PLASTICITY_MAT_H

// ============================================================================
// J2PlasticityMat.h - J2 (Von Mises) 弹塑性本构派生类
//
// 职责：
//   实现经典的 J2 流动塑性（带有等向强化和随动强化的预留接口）。
//   基于径向返回算法 (Radial Return) 求解弹塑性试探应力和塑性乘子。
//   高度耦合由于历史依赖而引申的状态变量体系。
// ============================================================================

#include "MechanicalMaterial.h"
#include "TypedField.h"
#include <string>
#include <yaml-cpp/yaml.h>

namespace PDCommon::Material {

class J2PlasticityMat : public MechanicalMaterial {
public:
  enum class HardeningType { Isotropic, Kinematic, Combined };

  explicit J2PlasticityMat(const std::string &name = "");
  ~J2PlasticityMat() override = default;

  // -----------------------------------------------------------------------
  // 实现 MechanicalMaterial 纯虚接口
  // -----------------------------------------------------------------------
  Eigen::Matrix3d
  ComputeEngineeringStress(const Eigen::Matrix3d &strain) const override;
  
  Eigen::Matrix3d ComputePK1Stress(const Eigen::Matrix3d &F, int particleId = -1) const override;

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

  // 塑性属性
  double yieldStress_{1e20};          // 初始屈服应力
  double hardeningModulus_{0.0};      // 等向/随动强化模量
  bool largeDeformation_{false};      // 是否使用大变形计算 (预留极分解插槽)
  HardeningType hardeningType_{HardeningType::Isotropic}; // 强化类型

  // 算法缓存：缓存所有材料辖区下的粒子数
  size_t numParticles_{0};

  // 状态变量裸指针 (SoA加速读取，无需每次都查表)
  // 1. 等效塑性应变 (EqPlasticStrain, 维数 1)
  double *eqPSOld_{nullptr};
  double *eqPSTrial_{nullptr};

  // 2. 塑性应变张量 (PlasticStrain, 维数 9)
  double *pSOld_{nullptr};
  double *pSTrial_{nullptr};

  // 3. 随动强化中的背应力 (BackStress, 维数 9) - 仅在非 Isotropic 模式下分配
  double *betaOld_{nullptr};
  double *betaTrial_{nullptr};

  // 4. 辅助推导输出：Von Mises 输出标量
  double *vonMises_{nullptr};

  // 面向 O(1) 交换的 TypedField 原生指针句柄缓存
  PDCommon::Field::TypedField<double> *fieldEqPSOld_{nullptr};
  PDCommon::Field::TypedField<double> *fieldEqPSTrial_{nullptr};
  PDCommon::Field::TypedField<double> *fieldPSOld_{nullptr};
  PDCommon::Field::TypedField<double> *fieldPSTrial_{nullptr};
  PDCommon::Field::TypedField<double> *fieldBetaOld_{nullptr};
  PDCommon::Field::TypedField<double> *fieldBetaTrial_{nullptr};

  void computeLameParameters();
};

} // namespace PDCommon::Material

#endif // PDCOMMON_MATERIAL_J2_PLASTICITY_MAT_H
