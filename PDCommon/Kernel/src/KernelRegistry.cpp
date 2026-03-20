// ============================================================================
// KernelRegistry.cpp - PD 积分核心全局注册中心实现
// ============================================================================

#include "KernelRegistry.h"
#include "Logger.h"

namespace PDCommon::Kernel {

KernelRegistry &KernelRegistry::getInstance() {
    static KernelRegistry instance;
    return instance;
}

void KernelRegistry::registerType(const std::string &type,
                                  KernelCreatorFunc creator) {
    registry_[type] = std::move(creator);
}

std::unique_ptr<PDKernel> KernelRegistry::create(const std::string &type) {
    auto it = registry_.find(type);
    if (it != registry_.end()) {
        return it->second();
    }
    LOG_ERROR("[KernelRegistry] Unknown kernel type: '" + type + "'");
    return nullptr;
}

bool KernelRegistry::hasType(const std::string &type) const {
    return registry_.find(type) != registry_.end();
}

std::vector<std::string> KernelRegistry::getRegisteredTypes() const {
    std::vector<std::string> types;
    types.reserve(registry_.size());
    for (const auto &[name, _] : registry_) {
        types.push_back(name);
    }
    return types;
}

} // namespace PDCommon::Kernel
