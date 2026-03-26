// ============================================================================
// MechanicalBC.cpp - 力学边界条件派生类实现 + 工厂注册
// ============================================================================

#include "MechanicalBC.h"
#include "BCRegistry.h"
#include "FieldManager.h"
#include <cmath>
#include <limits>

namespace PDCommon::BC {

static constexpr double UNCONSTRAINED_TOL = 1e20; // 超大数值视为无约束

// ===========================================================================
// DisplacementBC (Dirichlet)
// ===========================================================================
void DisplacementBC::initialize(PDCommon::Field::FieldManager &fieldManager,
                                int particleId,
                                const std::vector<double> &values) {
  displacement_ = fieldManager.getFieldAs<double>("Displacement");
  velocity_ = fieldManager.getFieldAs<double>("Velocity");
  acceleration_ = fieldManager.getFieldAs<double>("Acceleration");
  particleId_ = particleId;

  // 约定: values 如果是 [x, y, z]，数值大于 1e20 表示该方向无约束自由
  size_t dim = std::min(values.size(), static_cast<size_t>(3));
  for (size_t i = 0; i < dim; ++i) {
    if (std::abs(values[i]) < UNCONSTRAINED_TOL) {
      dispVal_[i] = values[i];
      applyDirs_[i] = true;
    } else {
      applyDirs_[i] = false;
    }
  }
}

void DisplacementBC::apply() {
  if (!displacement_)
    return;
  for (int d = 0; d < 3; ++d) {
    if (applyDirs_[d]) {
      // 钳制位移，并将关联方向的速度和加速度清零
      std::vector<double> &dispData = displacement_->getData();
      dispData[particleId_ * 3 + d] = dispVal_[d];

      if (velocity_) {
        std::vector<double> &velData = velocity_->getData();
        velData[particleId_ * 3 + d] = 0.0;
      }
      if (acceleration_) {
        std::vector<double> &accData = acceleration_->getData();
        accData[particleId_ * 3 + d] = 0.0;
      }
    }
  }
}

// ===========================================================================
// BodyForceBC (Neumann)
// ===========================================================================
void BodyForceBC::initialize(PDCommon::Field::FieldManager &fieldManager,
                             int particleId,
                             const std::vector<double> &values) {
  displacement_ = fieldManager.getFieldAs<double>("Displacement");
  velocity_ = fieldManager.getFieldAs<double>("Velocity");
  acceleration_ = fieldManager.getFieldAs<double>("Acceleration");
  particleId_ = particleId;

  size_t dim = std::min(values.size(), static_cast<size_t>(3));
  for (size_t i = 0; i < dim; ++i) {
    if (std::abs(values[i]) < UNCONSTRAINED_TOL) {
      accVal_[i] = values[i];
      applyDirs_[i] = true;
    } else {
      applyDirs_[i] = false;
    }
  }
}

void BodyForceBC::apply() {
  if (!acceleration_)
    return;
  for (int d = 0; d < 3; ++d) {
    if (applyDirs_[d]) {
      std::vector<double> &accData = acceleration_->getData();
      // 这里作为外加体力（加速度增量）累加
      accData[particleId_ * 3 + d] += accVal_[d];
    }
  }
}

// ===========================================================================
// VelocityBC (Dirichlet)
// ===========================================================================
void VelocityBC::initialize(PDCommon::Field::FieldManager &fieldManager,
                            int particleId, const std::vector<double> &values) {
  displacement_ = fieldManager.getFieldAs<double>("Displacement");
  velocity_ = fieldManager.getFieldAs<double>("Velocity");
  acceleration_ = fieldManager.getFieldAs<double>("Acceleration");
  particleId_ = particleId;

  size_t dim = std::min(values.size(), static_cast<size_t>(3));
  for (size_t i = 0; i < dim; ++i) {
    if (std::abs(values[i]) < UNCONSTRAINED_TOL) {
      velVal_[i] = values[i];
      applyDirs_[i] = true;
    } else {
      applyDirs_[i] = false;
    }
  }
}

void VelocityBC::apply() {
  if (!velocity_)
    return;
  for (int d = 0; d < 3; ++d) {
    if (applyDirs_[d]) {
      std::vector<double> &velData = velocity_->getData();
      velData[particleId_ * 3 + d] = velVal_[d];
    }
  }
}

// ===========================================================================
// PressureBC (Neumann)
// ===========================================================================
void PressureBC::initialize(PDCommon::Field::FieldManager &fieldManager,
                            int particleId, const std::vector<double> &values) {
  displacement_ = fieldManager.getFieldAs<double>("Displacement");
  velocity_ = fieldManager.getFieldAs<double>("Velocity");
  acceleration_ = fieldManager.getFieldAs<double>("Acceleration");
  particleId_ = particleId;

  size_t dim = std::min(values.size(), static_cast<size_t>(3));
  for (size_t i = 0; i < dim; ++i) {
    if (std::abs(values[i]) < UNCONSTRAINED_TOL) {
      pressVal_[i] = values[i];
      applyDirs_[i] = true;
    } else {
      applyDirs_[i] = false;
    }
  }
}

void PressureBC::apply() {
  if (!acceleration_)
    return;
  for (int d = 0; d < 3; ++d) {
    if (applyDirs_[d]) {
      std::vector<double> &accData = acceleration_->getData();
      // 这里作为外加体力（加速度增量）累加
      accData[particleId_ * 3 + d] += pressVal_[d];
    }
  }
}

// ============================================================================
// 编译期工厂注册：将具体子类以 .grpd 中的 Type 名注册到 BCRegistry
// ============================================================================

REGISTER_BC_TYPE(DISP, [](const std::string &name) {
  return std::make_unique<DisplacementBC>(name);
});

REGISTER_BC_TYPE(BODY_FORCE, [](const std::string &name) {
  return std::make_unique<BodyForceBC>(name);
});

REGISTER_BC_TYPE(VELOCITY, [](const std::string &name) {
  return std::make_unique<VelocityBC>(name);
});

REGISTER_BC_TYPE(PRESSURE, [](const std::string &name) {
  return std::make_unique<PressureBC>(name);
});

} // namespace PDCommon::BC
