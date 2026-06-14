#ifndef PDCOMMON_MATERIAL_CREEP_MATERIAL_BASE_H
#define PDCOMMON_MATERIAL_CREEP_MATERIAL_BASE_H

#include "MechanicalMaterial.h"
#include <Eigen/Dense>

namespace PDCommon::Field {
class FieldManager;
template <typename T> class TypedField;
}

namespace PDCommon::Material {

class CreepMaterialBase : public MechanicalMaterial {
public:
  using MechanicalMaterial::MechanicalMaterial;
  virtual ~CreepMaterialBase() = default;

  void initialize(const YAML::Node &matNode) override;
  void allocateStateVariables(PDCommon::Field::FieldManager &fm) override;
  void bindStateVariables(PDCommon::Field::FieldManager &fm) override;
  void commitState() override;
  size_t getNumStateVariables() const override { return 0; }
  void evaluate() override {}

  Eigen::Matrix3d
  ComputeEngineeringStress(const Eigen::Matrix3d &strain, PDCommon::Core::PDContext *ctx = nullptr) const override;

  Eigen::Matrix3d ComputePK1Stress(const Eigen::Matrix3d &F, int particleId = -1, int stateMode = 0, PDCommon::Core::PDContext *ctx = nullptr) const override;

  double getDensity() const override { return density_; }
  double getYoungsModulus() const override { return E_; }
  double getPoissonsRatio() const override { return nu_; }

protected:
  // 纯虚函数：计算等效蠕变应变增量
  virtual double computeDeltaCreepStrain(double q_trial, double dt, double t, double eq_cr_old) const = 0;

  double A_{0.0};
  double n_{1.0};
  
  double density_{0.0};
  double E_{0.0};
  double nu_{0.0};
  double mu_{0.0};
  double lambda_{0.0};
  double bulkModulus_{0.0};
  bool largeDeformation_{false};

  // 状态变量场对象指针，用于 commitState 同步
  PDCommon::Field::TypedField<double> *fieldEqCrOld_{nullptr};
  PDCommon::Field::TypedField<double> *fieldEqCrTrial_{nullptr};
  PDCommon::Field::TypedField<double> *fieldCrOld_{nullptr};
  PDCommon::Field::TypedField<double> *fieldCrTrial_{nullptr};

  // 裸指针，用于计算
  double *eqCrOld_{nullptr};
  double *eqCrTrial_{nullptr};
  double *crOld_{nullptr};
  double *crTrial_{nullptr};
};

} // namespace PDCommon::Material

#endif // PDCOMMON_MATERIAL_CREEP_MATERIAL_BASE_H
