// ============================================================================
// EngineRegistry.cpp - 求解器全局注册中心实现
// ============================================================================

#include "EngineRegistry.h"
#include "Logger.h"
#include <stdexcept>

namespace Src::Engine {

// ---------------------------------------------------------------------------
// 单例实例获取
// ---------------------------------------------------------------------------
EngineRegistry &EngineRegistry::getInstance() {
  static EngineRegistry instance;
  return instance;
}

// ---------------------------------------------------------------------------
// 注册求解器工厂函数
// ---------------------------------------------------------------------------
void EngineRegistry::registerType(const std::string &type,
                                  EngineCreatorFunc creator) {
  if (registry_.find(type) != registry_.end()) {
    LOG_WARNING("[EngineRegistry] Solver type '" + type +
                "' already registered, overwriting.");
  }
  registry_[type] = creator;
}

// ---------------------------------------------------------------------------
// 通过工厂创建引擎实例
// ---------------------------------------------------------------------------
std::unique_ptr<Engine> EngineRegistry::create(const std::string &type) {
  auto it = registry_.find(type);
  if (it == registry_.end()) {
    LOG_ERROR("[EngineRegistry] Create Engine failed: unknown type '" + type +
              "'!");
    throw std::runtime_error("Unknown Engine type: " + type);
  }
  LOG_INFO("[EngineRegistry] Creating Engine instance: " + type);
  return it->second();
}

// ---------------------------------------------------------------------------
// 检查是否已注册某类型
// ---------------------------------------------------------------------------
bool EngineRegistry::hasType(const std::string &type) const {
  return registry_.find(type) != registry_.end();
}

// ---------------------------------------------------------------------------
// 获取所有已注册的类型名列表
// ---------------------------------------------------------------------------
std::vector<std::string> EngineRegistry::getRegisteredTypes() const {
  std::vector<std::string> types;
  types.reserve(registry_.size());
  for (const auto &[key, _] : registry_) {
    types.push_back(key);
  }
  return types;
}

} // namespace Src::Engine
