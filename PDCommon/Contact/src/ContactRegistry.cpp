#include "ContactRegistry.h"
#include "Logger.h"

namespace PDCommon::Contact {

ContactRegistry &ContactRegistry::getInstance() {
  static ContactRegistry instance;
  return instance;
}

void ContactRegistry::registerForceLawType(const std::string &type,
                                           ForceLawCreatorFunc creator) {
  if (forceLawMap_.find(type) == forceLawMap_.end()) {
    forceLawMap_[type] = creator;
  }
}

void ContactRegistry::registerContactType(const std::string &type,
                                          ContactCreatorFunc creator) {
  if (contactMap_.find(type) == contactMap_.end()) {
    contactMap_[type] = creator;
  }
}

std::unique_ptr<IContactAlgorithm>
ContactRegistry::createContact(const std::string &type, const std::string &name,
                               const std::string &forceLawType) {

  auto contactIt = contactMap_.find(type);
  if (contactIt == contactMap_.end()) {
    LOG_ERROR("[ContactRegistry] Unknown Contact Type: '" + type + "'.");
    return nullptr;
  }

  // 可选提取 ForceLaw（仅在传入了 forceLawType 并且在字典里有注册时才生成）
  std::unique_ptr<IContactForceLaw> forceLaw = nullptr;
  auto flIt = forceLawMap_.find(forceLawType);
  if (flIt != forceLawMap_.end()) {
    forceLaw = flIt->second();
  } else if (type == "NTN" || type == "NTS") { 
    // NTN、NTS 等明确需要 ForceLaw 的组件如果没有查到，给出警告但不阻拦算法组装（组件内部可接盘报错）
    LOG_WARNING("[ContactRegistry] ForceLaw '" + forceLawType +
                "' not found or not mapped. Proceeding with null force law...");
  }

  // 交给顶层接触算法自行决定是否需要并且如何使用这个 forceLaw
  return contactIt->second(name, std::move(forceLaw));
}

std::vector<std::string> ContactRegistry::getRegisteredTypes() const {
  std::vector<std::string> types;
  for (const auto &p : contactMap_)
    types.push_back("[Type] " + p.first);
  for (const auto &p : forceLawMap_)
    types.push_back("[ForceLaw] " + p.first);
  return types;
}

} // namespace PDCommon::Contact
