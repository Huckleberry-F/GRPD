// ============================================================================
// ThermalFields.cpp - 热传导物理场配置器实现
// ============================================================================

#include "ThermalFields.h"
#include "FieldManager.h"
#include "FieldRegistry.h"
#include "Logger.h"
#include "PhysicsFieldRegistry.h"

namespace PDCommon::Field {

void ThermalFields::registerFields(FieldManager &fm) {
    auto &reg = FieldRegistry::getInstance();
    // 热传导核心场：温度 (标量)、变化率 (标量) 以及 热流密度 (标量)
    auto temperature = reg.createField("DoubleField", "Temperature", 1);
    auto tempRate    = reg.createField("DoubleField", "TempRate", 1);
    auto heatFlux    = reg.createField("DoubleField", "HeatFlux", 1);
    fm.addField(std::move(temperature));
    fm.addField(std::move(tempRate));
    fm.addField(std::move(heatFlux));
    LOG_INFO("[ThermalFields] Core fields registered: Temperature, TempRate, HeatFlux.");
}

// 静态注册到 PhysicsFieldRegistry
REGISTER_PHYSICS_FIELDS(Thermal, PDCommon::Field::ThermalFields)

} // namespace PDCommon::Field
