// ============================================================================
// MechanicalBC.cpp - 力学边界条件派生类实现 + 工厂注册 (ANSYS APDL 单自由度架构)
// ============================================================================

#include "MechanicalBC.h"
#include "BCRegistry.h"
#include "FieldManager.h"
#include <cmath>

namespace PDCommon::BC {

// ===========================================================================
// DisplacementBC (Dirichlet, 单轴)
// ===========================================================================
void DisplacementBC::initialize(PDCommon::Field::FieldManager &fieldManager,
                                int particleId,
                                const std::vector<double> &values) {
  displacement_ = fieldManager.getFieldAs<double>("Displacement");
  velocity_ = fieldManager.getFieldAs<double>("Velocity");
  acceleration_ = fieldManager.getFieldAs<double>("Acceleration");
  particleId_ = particleId;

  // values[0] 即为该单轴的约束值
  if (!values.empty()) {
    dispVal_ = values[0];
  }
}

void DisplacementBC::apply() {
  if (!displacement_)
    return;
  // 仅对单轴施加管控
  displacement_->set(particleId_, dispVal_, axis_);
  if (velocity_)
    velocity_->set(particleId_, 0.0, axis_);
  if (acceleration_)
    acceleration_->set(particleId_, 0.0, axis_);
}

void DisplacementBC::apply(double loadFactor) {
  if (!displacement_)
    return;
  // 按比例施加位移约束：u = prevVal + (dispVal - prevVal) * loadFactor
  double activeVal = prevVal_ + (dispVal_ - prevVal_) * loadFactor;
  displacement_->set(particleId_, activeVal, axis_);
  if (velocity_)
    velocity_->set(particleId_, 0.0, axis_);
  if (acceleration_)
    acceleration_->set(particleId_, 0.0, axis_);
}

void DisplacementBC::setTableNames(const std::vector<std::string> &names) {
  if (!names.empty()) {
    tableName_ = names[0];
  }
}

void DisplacementBC::commitEndStep() {
  if (!displacement_ || particleId_ < 0) return;
  // 从物理场中抽样当前粒子末态的真实位移，作为下一步的起点
  prevVal_ = displacement_->get(particleId_, axis_);
}

void DisplacementBC::applyWithTime(double currentTime) {
  if (!displacement_)
    return;
  double targetVal = dispVal_; // 默认回退值
  if (!tableName_.empty() && tableName_ != "None") {
    // Todo: 通过 TableManager 查表获取目标值
    targetVal = 0.0;
  }
  displacement_->set(particleId_, targetVal, axis_);
  if (velocity_)
    velocity_->set(particleId_, 0.0, axis_);
  if (acceleration_)
    acceleration_->set(particleId_, 0.0, axis_);
}

// ===========================================================================
// BodyForceBC (Neumann, 单轴)
// ===========================================================================
void BodyForceBC::initialize(PDCommon::Field::FieldManager &fieldManager,
                             int particleId,
                             const std::vector<double> &values) {
  displacement_ = fieldManager.getFieldAs<double>("Displacement");
  velocity_ = fieldManager.getFieldAs<double>("Velocity");
  acceleration_ = fieldManager.getFieldAs<double>("Acceleration");
  particleId_ = particleId;

  if (!values.empty()) {
    accVal_ = values[0];
  }
}

void BodyForceBC::apply() {
  if (!acceleration_)
    return;
  // 外加体力（加速度增量）累加到单轴
  acceleration_->add(particleId_, accVal_, axis_);
}

void BodyForceBC::apply(double loadFactor) {
  if (!acceleration_)
    return;
  // 按比例施加 Neumann 源项：a = prevVal_ + (accVal_ - prevVal_) * loadFactor
  double activeVal = prevVal_ + (accVal_ - prevVal_) * loadFactor;
  acceleration_->add(particleId_, activeVal, axis_);
}

void BodyForceBC::commitEndStep() {
  if (particleId_ < 0) return;
  prevVal_ = accVal_; // 没有单独保存源项的场，我们认为理想终态已经到达
}

// ===========================================================================
// VelocityBC (Dirichlet, 单轴)
// ===========================================================================
void VelocityBC::initialize(PDCommon::Field::FieldManager &fieldManager,
                            int particleId, const std::vector<double> &values) {
  displacement_ = fieldManager.getFieldAs<double>("Displacement");
  velocity_ = fieldManager.getFieldAs<double>("Velocity");
  acceleration_ = fieldManager.getFieldAs<double>("Acceleration");
  particleId_ = particleId;

  if (!values.empty()) {
    velVal_ = values[0];
  }
}

void VelocityBC::apply() {
  if (!velocity_)
    return;
  // 强制设定单轴速度值（Dirichlet 约束）
  velocity_->set(particleId_, velVal_, axis_);
  // 恒速边界：加速度需要强杀以阻断积分
  if (acceleration_)
    acceleration_->set(particleId_, 0.0, axis_);
}

void VelocityBC::apply(double loadFactor) {
  if (!velocity_)
    return;
  // 按比例施加速度约束：v = prevVal_ + (velVal_ - prevVal_) * loadFactor
  double activeVal = prevVal_ + (velVal_ - prevVal_) * loadFactor;
  velocity_->set(particleId_, activeVal, axis_);
  if (acceleration_)
    acceleration_->set(particleId_, 0.0, axis_);
}

void VelocityBC::commitEndStep() {
  if (!velocity_ || particleId_ < 0) return;
  // 从物理场中抽样当前粒子末态的真实速度，作为下一步的起点
  prevVal_ = velocity_->get(particleId_, axis_);
}

// ===========================================================================
// PressureBC (Neumann, 单轴)
// ===========================================================================
void PressureBC::initialize(PDCommon::Field::FieldManager &fieldManager,
                            int particleId, const std::vector<double> &values) {
  displacement_ = fieldManager.getFieldAs<double>("Displacement");
  velocity_ = fieldManager.getFieldAs<double>("Velocity");
  acceleration_ = fieldManager.getFieldAs<double>("Acceleration");
  particleId_ = particleId;

  if (!values.empty()) {
    pressVal_ = values[0];
  }
}

void PressureBC::apply() {
  if (!acceleration_)
    return;
  // 转换宏观面压 (MPa) 为隐式动力学底层的等效单点加速度
  double equivalentAcc = pressVal_ / (dx_ * density_ * massScale_);
  acceleration_->add(particleId_, equivalentAcc, axis_);
}

void PressureBC::apply(double loadFactor) {
  if (!acceleration_)
    return;
  // 按比例施加 Neumann 源项压强：P = prevVal_ + (pressVal_ - prevVal_) * loadFactor
  double activeVal = prevVal_ + (pressVal_ - prevVal_) * loadFactor;
  double equivalentAcc = activeVal / (dx_ * density_ * massScale_);
  acceleration_->add(particleId_, equivalentAcc, axis_);
}

void PressureBC::commitEndStep() {
  if (particleId_ < 0) return;
  prevVal_ = pressVal_; // 没有单独保存源项的场，认为理想终态已经到达
}

// ============================================================================
// 编译期工厂注册：ANSYS APDL 命名风格，自由度→单轴 BC 实例
// ============================================================================

// --- 位移约束 (UX / UY / UZ) ---
REGISTER_BC_TYPE(UX, [](const std::string &name) {
  return std::make_unique<DisplacementBC>(name, 0);
});
REGISTER_BC_TYPE(UY, [](const std::string &name) {
  return std::make_unique<DisplacementBC>(name, 1);
});
REGISTER_BC_TYPE(UZ, [](const std::string &name) {
  return std::make_unique<DisplacementBC>(name, 2);
});

// --- 速度约束 (VX / VY / VZ) ---
REGISTER_BC_TYPE(VX, [](const std::string &name) {
  return std::make_unique<VelocityBC>(name, 0);
});
REGISTER_BC_TYPE(VY, [](const std::string &name) {
  return std::make_unique<VelocityBC>(name, 1);
});
REGISTER_BC_TYPE(VZ, [](const std::string &name) {
  return std::make_unique<VelocityBC>(name, 2);
});

// --- 体力/加速度 (AX / AY / AZ) ---
REGISTER_BC_TYPE(AX, [](const std::string &name) {
  return std::make_unique<BodyForceBC>(name, 0);
});
REGISTER_BC_TYPE(AY, [](const std::string &name) {
  return std::make_unique<BodyForceBC>(name, 1);
});
REGISTER_BC_TYPE(AZ, [](const std::string &name) {
  return std::make_unique<BodyForceBC>(name, 2);
});

// --- 压力 (PX / PY / PZ) ---
REGISTER_BC_TYPE(PX, [](const std::string &name) {
  return std::make_unique<PressureBC>(name, 0);
});
REGISTER_BC_TYPE(PY, [](const std::string &name) {
  return std::make_unique<PressureBC>(name, 1);
});
REGISTER_BC_TYPE(PZ, [](const std::string &name) {
  return std::make_unique<PressureBC>(name, 2);
});

} // namespace PDCommon::BC
