#ifndef PDCOMMON_BC_MECHANICAL_BC_H
#define PDCOMMON_BC_MECHANICAL_BC_H

// ============================================================================
// MechanicalBC.h - 力学边界条件派生体系 (ANSYS APDL 单自由度架构)
// 责任：
// 1. MechanicalBC 作为力学边界条件的中间抽象类，继承 BC 基类
// 2. 每个 BC 实例仅对应单一轴向（Single-DOF）：UX/UY/UZ, VX/VY/VZ 等
// 3. 通过 REGISTER_BC_TYPE 宏在编译期自动注册到 BCRegistry
// ============================================================================

#include "BC.h"
#include "TypedField.h"

namespace PDCommon::BC {

/**
 * @brief 力学边界条件抽象中间类
 * 持有三个 TypedField<double> 指针（位移、速度、加速度）
 */
class MechanicalBC : public BC {
public:
  explicit MechanicalBC(const std::string &name)
      : BC(name), displacement_(nullptr), velocity_(nullptr),
        acceleration_(nullptr) {}

  virtual ~MechanicalBC() = default;

protected:
  PDCommon::Field::TypedField<double> *displacement_;
  PDCommon::Field::TypedField<double> *velocity_;
  PDCommon::Field::TypedField<double> *acceleration_;
};

/**
 * @brief 单轴位移边界条件 (Dirichlet)
 * 对应 APDL 中的 D, node, UX/UY/UZ, value
 * 每个实例仅约束单一轴向的位移
 */
class DisplacementBC : public MechanicalBC {
public:
  /// @param name 边界条件名称
  /// @param axis 工作轴向 (0=X, 1=Y, 2=Z)
  explicit DisplacementBC(const std::string &name, int axis = 0)
      : MechanicalBC(name), particleId_(-1), axis_(axis),
        dispVal_(0.0), prevVal_(0.0) {}

  void initialize(PDCommon::Field::FieldManager &fieldManager, int particleId,
                  const std::vector<double> &values) override;

  void apply() override;
  void apply(double loadFactor) override;
  void applyWithTime(double currentTime) override;
  void setTableNames(const std::vector<std::string> &names) override;

  bool isConstraint() const override {
    return true;
  } // Dirichlet: 直接设定位移值

  void commitEndStep() override;

private:
  int particleId_;
  int axis_;           // 工作轴 (0=X, 1=Y, 2=Z)
  double dispVal_;     // 位移约束值
  double prevVal_;     // 上一步的终态值（用于 Ramp 连续加载）
  std::string tableName_; // 绑定的表格名称
};

/**
 * @brief 单轴体力/加速度边界条件 (Neumann)
 * 对应 APDL 中的 F, node, FX/FY/FZ, value
 */
class BodyForceBC : public MechanicalBC {
public:
  explicit BodyForceBC(const std::string &name, int axis = 0)
      : MechanicalBC(name), particleId_(-1), axis_(axis), accVal_(0.0), prevVal_(0.0) {}

  void initialize(PDCommon::Field::FieldManager &fieldManager, int particleId,
                  const std::vector<double> &values) override;

  void apply() override;
  void apply(double loadFactor) override;
  void commitEndStep() override;

  bool isConstraint() const override {
    return false;
  } // Neumann: 向加速度场累加

private:
  int particleId_;
  int axis_;       // 工作轴 (0=X, 1=Y, 2=Z)
  double accVal_;  // 体力/加速度值
  double prevVal_; // 历史最终态值
};

/**
 * @brief 单轴速度边界条件 (Dirichlet)
 * 对应 APDL 中的 D, node, VX/VY/VZ, value (运动学约束)
 */
class VelocityBC : public MechanicalBC {
public:
  explicit VelocityBC(const std::string &name, int axis = 0)
      : MechanicalBC(name), particleId_(-1), axis_(axis), velVal_(0.0), prevVal_(0.0) {}

  void initialize(PDCommon::Field::FieldManager &fieldManager, int particleId,
                  const std::vector<double> &values) override;

  void apply() override;
  void apply(double loadFactor) override;
  void commitEndStep() override;

  bool isConstraint() const override {
    return true;
  } // Dirichlet: 强制覆盖速度值

private:
  int particleId_;
  int axis_;       // 工作轴 (0=X, 1=Y, 2=Z)
  double velVal_;  // 速度约束值
  double prevVal_; // 历史最终态值
};

/**
 * @brief 单轴压力边界条件 (Neumann)
 * 对应 APDL 中的压力施加
 */
class PressureBC : public MechanicalBC {
public:
  explicit PressureBC(const std::string &name, int axis = 0)
      : MechanicalBC(name), particleId_(-1), axis_(axis), pressVal_(0.0), prevVal_(0.0) {}

  void initialize(PDCommon::Field::FieldManager &fieldManager, int particleId,
                  const std::vector<double> &values) override;

  void apply() override;
  void apply(double loadFactor) override;
  void commitEndStep() override;

  void setScalingFactors(double dx, double density, double massScale) override {
    dx_ = dx;
    density_ = density;
    massScale_ = massScale;
  }

  bool isConstraint() const override {
    return false;
  } // Neumann: 向加速度场累加

private:
  int particleId_;
  int axis_;         // 工作轴 (0=X, 1=Y, 2=Z)
  double pressVal_;  // 面压值 (MPa)
  double prevVal_;   // 历史最终态值
  double dx_ = 1.0;
  double density_ = 1.0;
  double massScale_ = 1.0;
};

} // namespace PDCommon::BC

#endif // PDCOMMON_BC_MECHANICAL_BC_H
