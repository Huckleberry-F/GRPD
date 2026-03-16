// ============================================================================
// ThermalFields.cpp - 热传导物理场配置器实现
// ============================================================================

#include "ThermalFields.h"
#include "FieldManager.h"
#include "Logger.h"
#include "PhysicsFieldRegistry.h"

namespace PDCommon::Field {

void ThermalFields::registerFields(FieldManager &fm) {
    // 热传导核心场：温度 (标量)、变化率 (标量) 以及 热流密度 (标量)
    fm.registerField<double>("Temperature", 1);
    fm.registerField<double>("TempRate", 1);
    fm.registerField<double>("HeatFlux", 1);
    LOG_INFO("[ThermalFields] Core fields registered: Temperature, TempRate, HeatFlux.");
}

// 静态注册到 PhysicsFieldRegistry
REGISTER_PHYSICS_FIELDS(Thermal, PDCommon::Field::ThermalFields)

} // namespace PDCommon::Field
