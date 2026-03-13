// ============================================================================
// BCRegistry.cpp - 边界条件全局注册中心实现
// ============================================================================

#include "BCRegistry.h"
#include "Logger.h"
#include <stdexcept>

namespace GRPD::BC {

// ---------------------------------------------------------------------------
// 单例实例获取
// ---------------------------------------------------------------------------
BCRegistry &BCRegistry::getInstance() {
    static BCRegistry instance;
    return instance;
}

// ---------------------------------------------------------------------------
// 注册边界条件工厂函数
// ---------------------------------------------------------------------------
void BCRegistry::registerBCType(const std::string &type,
                                BCCreatorFunc creator) {
    if (registryMap_.find(type) != registryMap_.end()) {
        LOG_WARNING("[BCRegistry] BC type '" + type +
                    "' already registered, overwriting.");
    } else {
        LOG_INFO("[BCRegistry] BC type '" + type + "' registered.");
    }
    registryMap_[type] = creator;
}

// ---------------------------------------------------------------------------
// 通过工厂创建边界条件实例（不保管，返回所有权）
// ---------------------------------------------------------------------------
std::unique_ptr<BC> BCRegistry::createBC(const std::string &type,
                                          const std::string &name) {
    auto it = registryMap_.find(type);
    if (it == registryMap_.end()) {
        LOG_ERROR("[BCRegistry] Create BC failed: unknown type '" + type + "'!");
        throw std::runtime_error("Unknown BC type: " + type);
    }
    return it->second(name);
}

} // namespace GRPD::BC
