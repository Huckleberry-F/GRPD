// ============================================================================
// TimeIntegratorRegistry.cpp - 时间积分器全局注册中心实现
// ============================================================================

#include "TimeIntegratorRegistry.h"
#include "Logger.h"

namespace Src::Integration {

TimeIntegratorRegistry &TimeIntegratorRegistry::getInstance() {
    static TimeIntegratorRegistry instance;
    return instance;
}

void TimeIntegratorRegistry::registerType(const std::string &type,
                                          TimeIntegratorCreator creator) {
    registry_[type] = std::move(creator);
}

std::unique_ptr<TimeIntegrator>
TimeIntegratorRegistry::create(const std::string &type) {
    auto it = registry_.find(type);
    if (it != registry_.end()) {
        return it->second();
    }
    LOG_ERROR("[TimeIntegratorRegistry] Unknown integrator type: '" + type +
              "'");
    return nullptr;
}

bool TimeIntegratorRegistry::hasType(const std::string &type) const {
    return registry_.find(type) != registry_.end();
}

std::vector<std::string> TimeIntegratorRegistry::getRegisteredTypes() const {
    std::vector<std::string> types;
    types.reserve(registry_.size());
    for (const auto &[name, _] : registry_) {
        types.push_back(name);
    }
    return types;
}

} // namespace Src::Integration
