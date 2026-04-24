#ifndef PDCOMMON_MATERIAL_LINEAR_ELASTIC_MAT_H
#define PDCOMMON_MATERIAL_LINEAR_ELASTIC_MAT_H

// ============================================================================
// LinearElasticMat.h - 各向同性线弹性本构派生类
//
// 职责：
//   实现经典的线弹性胡克定律本构关系。
//   具备将变形梯度转为第一类 P-K 应力的能力。
// ============================================================================

#include "MechanicalMaterial.h"
#include <string>
#include <yaml-cpp/yaml.h>

namespace PDCommon::Material {

class LinearElasticMat : public MechanicalMaterial {
public:
  explicit LinearElasticMat(const std::string &name = "");
  ~LinearElasticMat() override = default;

  // -----------------------------------------------------------------------
  // 实现 MechanicalMaterial 纯虚接口
  // -----------------------------------------------------------------------
  Eigen::Matrix3d
  ComputeEngineeringStress(const Eigen::Matrix3d &F) const override;
  Eigen::Matrix3d ComputePK1Stress(const Eigen::Matrix3d &F,
                                   int particleId = -1, int stateMode = 0) const override;

  // -----------------------------------------------------------------------
  // 实现 Material 基类虚函数
  // -----------------------------------------------------------------------
  void initialize(const YAML::Node &matNode) override;
  void allocateStateVariables(PDCommon::Field::FieldManager &fm) override;
  size_t getNumStateVariables() const override;
  void evaluate() override;

  double getDensity() const override;
  double getYoungsModulus() const override;
  double getPoissonsRatio() const override;
  // -----------------------------------------------------------------------
  // Setter/Getter
  // -----------------------------------------------------------------------
  void setDensity(double rho);
  void setYoungsModulus(double E);
  void setPoissonsRatio(double nu);

private:
  double density_{-1.0};
  double youngsModulus_{-1.0};
  double poissonsRatio_{-1.0};

  // 派生拉梅常数
  double lambda_{0.0};
  double mu_{0.0};

  void computeLameParameters();
};

} // namespace PDCommon::Material

#endif // PDCOMMON_MATERIAL_LINEAR_ELASTIC_MAT_H
