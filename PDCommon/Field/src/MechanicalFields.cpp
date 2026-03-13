// ============================================================================
// MechanicalFields.cpp - 力学物理场配置器实现
// ============================================================================

#include "MechanicalFields.h"
#include "FieldManager.h"
#include "Logger.h"
#include "PhysicsFieldRegistry.h"

namespace GRPD::Field {

void MechanicalFields::registerFields(FieldManager &fm) {
    // 力学核心场：位移、速度、加速度（均为三维矢量场）
    fm.registerField<double>("Displacement", 3);
    fm.registerField<double>("Velocity", 3);
    fm.registerField<double>("Acceleration", 3);
    LOG_INFO("[MechanicalFields] Core fields registered: "
             "Displacement, Velocity, Acceleration.");
}

// 静态注册到 PhysicsFieldRegistry
REGISTER_PHYSICS_FIELDS(Mechanical, GRPD::Field::MechanicalFields)

} // namespace GRPD::Field
