// ============================================================================
// MechanicalFields.cpp - 力学物理场配置器实现
// ============================================================================

#include "MechanicalFields.h"
#include "FieldManager.h"
#include "FieldRegistry.h"
#include "Logger.h"
#include "PhysicsFieldRegistry.h"

namespace PDCommon::Field {

void MechanicalFields::registerFields(FieldManager &fm) {
    auto &reg = FieldRegistry::getInstance();
    // 力学核心场：位移、速度、加速度（均为三维矢量场）
    auto displacement = reg.createField("DoubleField", "Displacement", 3);
    auto velocity     = reg.createField("DoubleField", "Velocity", 3);
    auto acceleration = reg.createField("DoubleField", "Acceleration", 3);
    fm.addField(std::move(displacement));
    fm.addField(std::move(velocity));
    fm.addField(std::move(acceleration));
    LOG_INFO("[MechanicalFields] Core fields registered: "
             "Displacement, Velocity, Acceleration.");
}

// 静态注册到 PhysicsFieldRegistry
REGISTER_PHYSICS_FIELDS(Mechanical, PDCommon::Field::MechanicalFields)

} // namespace PDCommon::Field
