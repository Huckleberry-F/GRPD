// ============================================================================
// PhysicsFieldRegistry.cpp - 物理场配置器注册中心实现
// ============================================================================

#include "PhysicsFieldRegistry.h"
#include "Logger.h"
#include <stdexcept>

namespace PDCommon::Field {

PhysicsFieldRegistry &PhysicsFieldRegistry::getInstance() {
    static PhysicsFieldRegistry instance;
    return instance;
}

void PhysicsFieldRegistry::registerType(const std::string &type,
                                         PhysicsFieldsCreatorFunc creator) {
    if (registry_.find(type) != registry_.end()) {
        LOG_WARNING("[PhysicsFieldRegistry] Type '" + type +
                    "' already registered, overwriting.");
    }
    registry_[type] = creator;
}

std::unique_ptr<PhysicsFields>
PhysicsFieldRegistry::create(const std::string &type) {
    auto it = registry_.find(type);
    if (it == registry_.end()) {
        LOG_ERROR("[PhysicsFieldRegistry] Unknown physics type: '" + type +
                  "'!");
        throw std::runtime_error("Unknown PhysicsFields type: " + type);
    }
    return it->second();
}

bool PhysicsFieldRegistry::hasType(const std::string &type) const {
    return registry_.find(type) != registry_.end();
}

std::vector<std::string> PhysicsFieldRegistry::getRegisteredTypes() const {
    std::vector<std::string> types;
    types.reserve(registry_.size());
    for (const auto &[key, _] : registry_) {
        types.push_back(key);
    }
    return types;
}

} // namespace PDCommon::Field
