#ifndef PDCOMMON_FIELD_THERMAL_FIELDS_H
#define PDCOMMON_FIELD_THERMAL_FIELDS_H

// ============================================================================
// ThermalFields.h - 热传导物理场配置器
// 责任：注册热传导问题所需的核心场 (Temperature, TempRate)
// ============================================================================

#include "PhysicsFields.h"

namespace PDCommon::Field {

class ThermalFields : public PhysicsFields {
public:
    ThermalFields() : PhysicsFields("Thermal") {}
    ~ThermalFields() override = default;

    /// @brief 注册热传导核心场
    void registerFields(FieldManager &fm) override;
};

} // namespace PDCommon::Field

#endif // PDCOMMON_FIELD_THERMAL_FIELDS_H
