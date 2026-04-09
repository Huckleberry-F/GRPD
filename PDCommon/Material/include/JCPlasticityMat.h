#ifndef PDCOMMON_MATERIAL_JC_PLASTICITY_MAT_H
#define PDCOMMON_MATERIAL_JC_PLASTICITY_MAT_H

// ============================================================================
// JCPlasticityMat.h - Johnson-Cook 塑性损伤本构
//
// 继承自 J2PlasticityMat，在求得 J2 弹塑性本构基础上，追加 JC 损伤变量累积
// 生成 Damage_Trial 场用于统一的 Fracture 断裂判定。
// ============================================================================

#include "J2PlasticityMat.h"

namespace PDCommon::Material {

class JCPlasticityMat : public J2PlasticityMat {
public:
  explicit JCPlasticityMat(const std::string &name = "");
  ~JCPlasticityMat() override = default;

  // -----------------------------------------------------------------------
  // 实现 Material 基类虚函数
  // -----------------------------------------------------------------------
  void initialize(const YAML::Node &matNode) override;
  void allocateStateVariables(PDCommon::Field::FieldManager &fm) override;
  void bindStateVariables(PDCommon::Field::FieldManager &fm) override;
  void commitState() override;

  Eigen::Matrix3d ComputePK1Stress(const Eigen::Matrix3d &F, int particleId = -1) const override;

private:
  // JC 损伤参数
  double D1_{0.15};
  double D2_{0.0};
  double D3_{0.0};
  
  bool useTriaxiality_{false};

  // 损伤场裸指针
  double *damageOld_{nullptr};
  double *damageTrial_{nullptr};
};

} // namespace PDCommon::Material

#endif // PDCOMMON_MATERIAL_JC_PLASTICITY_MAT_H
