#ifndef PDCOMMON_BC_MECHANICAL_BC_H
#define PDCOMMON_BC_MECHANICAL_BC_H

// ============================================================================
// MechanicalBC.h - 力学边界条件派生体系
// 责任：
// 1. MechanicalBC 作为力学边界条件的中间抽象类，继承 BC 基类
// 2. 派生出 DisplacementBC (Dirichlet), BodyForceBC / AccelerationBC (Neumann)
// 架构规约：通过 REGISTER_BC_TYPE 宏在编译期自动注册到 BCRegistry
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

  std::string tableName_[3] = {"", "", ""};
};

/**
 * @brief 位移边界条件 (Dirichlet)
 * u(x, t) = u_fixed
 * 也相应地将速度钳制为 0 (静态约束)
 */
class DisplacementBC : public MechanicalBC {
public:
  explicit DisplacementBC(const std::string &name)
      : MechanicalBC(name), particleId_(-1) {
    dispVal_[0] = 0.0;
    dispVal_[1] = 0.0;
    dispVal_[2] = 0.0;
    applyDirs_[0] = false;
    applyDirs_[1] = false;
    applyDirs_[2] = false;
  }

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
  size_t particleId_ = 0;
  bool applyDirs_[3] = {true, true, true};
  double dispVal_[3] = {0.0, 0.0, 0.0};
  double prevVal_[3] = {0.0, 0.0, 0.0};
  std::string tableName_[3];
};

/**
 * @brief 体力/加速度边界条件 (Neumann)
 * a(x, t) = a_fixed (例如重力场 9.8)
 */
class BodyForceBC : public MechanicalBC {
public:
  explicit BodyForceBC(const std::string &name)
      : MechanicalBC(name), particleId_(-1) {
    accVal_[0] = 0.0;
    accVal_[1] = 0.0;
    accVal_[2] = 0.0;
    applyDirs_[0] = false;
    applyDirs_[1] = false;
    applyDirs_[2] = false;
  }

  void initialize(PDCommon::Field::FieldManager &fieldManager, int particleId,
                  const std::vector<double> &values) override;

  void apply() override;
  bool isConstraint() const override {
    return false;
  } // Neumann: 向加速度场累加

private:
  int particleId_;
  double accVal_[3];
  bool applyDirs_[3];
};
/**
 * @brief 速度边界条件 (Dirichlet)
 * v(x, t) = v_fixed
 */
class VelocityBC : public MechanicalBC {
public:
  explicit VelocityBC(const std::string &name)
      : MechanicalBC(name), particleId_(-1) {
    velVal_[0] = 0.0;
    velVal_[1] = 0.0;
    velVal_[2] = 0.0;
    applyDirs_[0] = false;
    applyDirs_[1] = false;
    applyDirs_[2] = false;
  }

  void initialize(PDCommon::Field::FieldManager &fieldManager, int particleId,
                  const std::vector<double> &values) override;

  void apply() override;
  void apply(double loadFactor) override;
  bool isConstraint() const override {
    return false;
  } // Dirichlet: 直接设定速度值

private:
  int particleId_;
  double velVal_[3];
  bool applyDirs_[3];
};

/**
 * @brief 压力边界条件 (Neumann)
 * p(x, t) = p_fixed
 */
class PressureBC : public MechanicalBC {
public:
  explicit PressureBC(const std::string &name)
      : MechanicalBC(name), particleId_(-1) {
    pressVal_[0] = 0.0;
    pressVal_[1] = 0.0;
    pressVal_[2] = 0.0;
    applyDirs_[0] = false;
    applyDirs_[1] = false;
    applyDirs_[2] = false;
  }

  void initialize(PDCommon::Field::FieldManager &fieldManager, int particleId,
                  const std::vector<double> &values) override;

  void apply() override;
  bool isConstraint() const override {
    return false;
  } // Neumann: 向加速度场累加

private:
  int particleId_;
  double pressVal_[3];
  bool applyDirs_[3];
};

} // namespace PDCommon::BC

#endif // PDCOMMON_BC_MECHANICAL_BC_H
