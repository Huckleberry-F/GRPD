// ============================================================================
// FractureRegistry.cpp
// ============================================================================

#include "FractureRegistry.h"
#include "Logger.h"

namespace PDCommon::Fracture {

FractureRegistry &FractureRegistry::getInstance() {
  static FractureRegistry instance;
  return instance;
}

void FractureRegistry::registerFactory(const std::string &name, Creator creator) {
  if (factories_.find(name) != factories_.end()) {
    LOG_ERROR("[FractureRegistry] Fracture model '" + name + "' is already registered.");
    return;
  }
  factories_[name] = std::move(creator);
}

std::unique_ptr<FractureModel> FractureRegistry::create(const std::string &name) const {
  auto it = factories_.find(name);
  if (it == factories_.end()) {
    LOG_ERROR("[FractureRegistry] Fracture model '" + name + "' not found.");
    return nullptr;
  }
  return it->second();
}

bool FractureRegistry::contains(const std::string &name) const {
  return factories_.find(name) != factories_.end();
}

} // namespace PDCommon::Fracture
