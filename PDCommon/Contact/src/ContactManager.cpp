#include "ContactManager.h"
#include <iostream>

namespace PDCommon::Contact {

// ---------------------------------------------------------------------------
// addContactPair: 添加接触对实例（所有权转移）
// ---------------------------------------------------------------------------
void ContactManager::addContactPair(std::unique_ptr<IContactAlgorithm> contact) {
  std::string name = contact->getName();
  contactPairs_[name] = std::move(contact);
}

// ---------------------------------------------------------------------------
// getContactPair: 按名称查找接触对
// ---------------------------------------------------------------------------
IContactAlgorithm *ContactManager::getContactPair(const std::string &name) {
  auto it = contactPairs_.find(name);
  return (it != contactPairs_.end()) ? it->second.get() : nullptr;
}

const IContactAlgorithm *ContactManager::getContactPair(const std::string &name) const {
  auto it = contactPairs_.find(name);
  return (it != contactPairs_.end()) ? it->second.get() : nullptr;
}

// ---------------------------------------------------------------------------
// hasContactPair: 检查接触对是否存在
// ---------------------------------------------------------------------------
bool ContactManager::hasContactPair(const std::string &name) const {
  return contactPairs_.find(name) != contactPairs_.end();
}

// ---------------------------------------------------------------------------
// clear: 清空所有接触对
// ---------------------------------------------------------------------------
void ContactManager::clear() {
  contactPairs_.clear();
}

} // namespace PDCommon::Contact
