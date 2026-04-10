#include "ContactRegistry.h"
#include <iostream>

namespace PDCommon::Contact {

ContactRegistry &ContactRegistry::getInstance() {
  static ContactRegistry instance;
  return instance;
}

void ContactRegistry::registerContactType(const std::string &type,
                                          ContactCreatorFunc creator) {
  if (registryMap_.find(type) == registryMap_.end()) {
    registryMap_[type] = creator;
  }
}

std::unique_ptr<IContactAlgorithm>
ContactRegistry::createContact(const std::string &type,
                               const std::string &name) {
  auto it = registryMap_.find(type);
  if (it != registryMap_.end()) {
    return it->second(name);
  }
  return nullptr;
}

std::vector<std::string> ContactRegistry::getRegisteredTypes() const {
  std::vector<std::string> types;
  for (const auto &pair : registryMap_) {
    types.push_back(pair.first);
  }
  return types;
}

} // namespace PDCommon::Contact
