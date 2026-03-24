#include "StabilizerRegistry.h"
#include "Logger.h"


namespace PDCommon::Kernel {

StabilizerRegistry &StabilizerRegistry::getInstance() {
  static StabilizerRegistry instance;
  return instance;
}

void StabilizerRegistry::registerStabilizer(const std::string &name,
                                            StabilizerCreator creator) {
  if (registryMap_.find(name) != registryMap_.end()) {
    LOG_ERROR("[StabilizerRegistry] Stabilizer '" + name + "' is already registered!");
    return;
  }
  registryMap_[name] = std::move(creator);
  LOG_INFO("[StabilizerRegistry] Registered Stabilizer: " + name);
}

std::unique_ptr<Stabilizer> StabilizerRegistry::create(const std::string &name) const {
  auto it = registryMap_.find(name);
  if (it != registryMap_.end()) {
    return it->second();
  }
  LOG_ERROR("[StabilizerRegistry] Requested Stabilizer '" + name + "' not found!");
  return nullptr;
}

bool StabilizerRegistry::contains(const std::string &name) const {
  return registryMap_.find(name) != registryMap_.end();
}

} // namespace PDCommon::Kernel
