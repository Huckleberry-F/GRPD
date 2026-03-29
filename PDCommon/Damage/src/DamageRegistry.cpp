// ============================================================================
// DamageRegistry.cpp
// ============================================================================

#include "DamageRegistry.h"
#include "Logger.h"

namespace PDCommon::Damage {

DamageRegistry &DamageRegistry::getInstance() {
  static DamageRegistry instance;
  return instance;
}

void DamageRegistry::registerFactory(const std::string &name, Creator creator) {
  if (factories_.find(name) != factories_.end()) {
    LOG_ERROR("[DamageRegistry] Damage model '" + name + "' is already registered.");
    return;
  }
  factories_[name] = std::move(creator);
}

std::unique_ptr<DamageModel> DamageRegistry::create(const std::string &name) const {
  auto it = factories_.find(name);
  if (it == factories_.end()) {
    LOG_ERROR("[DamageRegistry] Damage model '" + name + "' not found.");
    return nullptr;
  }
  return it->second();
}

bool DamageRegistry::contains(const std::string &name) const {
  return factories_.find(name) != factories_.end();
}

} // namespace PDCommon::Damage
