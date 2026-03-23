#ifndef PDCOMMON_FIELD_MECHANICAL_FIELDS_H
#define PDCOMMON_FIELD_MECHANICAL_FIELDS_H

// ============================================================================
// MechanicalFields.h - 力学物理场配置器
// 责任：注册力学问题所需的核心场 (Displacement, Velocity, Acceleration)
// ============================================================================

#include "PhysicsFields.h"

namespace PDCommon::Field {

class MechanicalFields : public PhysicsFields {
public:
    MechanicalFields() : PhysicsFields("Mechanical") {}
    ~MechanicalFields() override = default;

    /// @brief 注册力学核心场
    void registerFields(FieldManager &fm) override;
};

} // namespace PDCommon::Field

#endif // PDCOMMON_FIELD_MECHANICAL_FIELDS_H
