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

std::vector<std::string> StabilizerRegistry::getRegisteredTypes() const {
  std::vector<std::string> types;
  types.reserve(registryMap_.size());
  for (const auto &pair : registryMap_) {
    types.push_back(pair.first);
  }
  return types;
}

} // namespace PDCommon::Kernel
