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

void ContactRegistry::registerEvaluatorType(const std::string &type,
                                            EvaluatorCreatorFunc creator) {
  if (evaluatorMap_.find(type) == evaluatorMap_.end()) {
    evaluatorMap_[type] = creator;
  }
}

void ContactRegistry::registerFrictionLawType(const std::string &type,
                                              FrictionLawCreatorFunc creator) {
  if (frictionLawMap_.find(type) == frictionLawMap_.end()) {
    frictionLawMap_[type] = creator;
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

  std::unique_ptr<IContactForceLaw> forceLaw = nullptr;
  auto flIt = forceLawMap_.find(forceLawType);
  if (flIt != forceLawMap_.end()) {
    forceLaw = flIt->second();
  }

  return contactIt->second(name, std::move(forceLaw));
}

std::unique_ptr<IContactForceLaw>
ContactRegistry::createForceLaw(const std::string &type) {
  auto it = forceLawMap_.find(type);
  if (it == forceLawMap_.end())
    return nullptr;
  return it->second();
}

std::unique_ptr<IContactEvaluator>
ContactRegistry::createEvaluator(const std::string &type) {
  auto it = evaluatorMap_.find(type);
  if (it == evaluatorMap_.end()) {
    LOG_ERROR("[ContactRegistry] Unknown Evaluator Type: '" + type + "'.");
    return nullptr;
  }
  return it->second();
}

std::unique_ptr<IFrictionLaw>
ContactRegistry::createFrictionLaw(const std::string &type) {
  auto it = frictionLawMap_.find(type);
  if (it == frictionLawMap_.end()) {
    LOG_ERROR("[ContactRegistry] Unknown FrictionLaw Type: '" + type + "'.");
    return nullptr;
  }
  return it->second();
}

std::vector<std::string> ContactRegistry::getRegisteredTypes() const {
  std::vector<std::string> types;
  for (const auto &p : contactMap_)
    types.push_back("[Type] " + p.first);
  for (const auto &p : forceLawMap_)
    types.push_back("[ForceLaw] " + p.first);
  for (const auto &p : evaluatorMap_)
    types.push_back("[Evaluator] " + p.first);
  for (const auto &p : frictionLawMap_)
    types.push_back("[FrictionLaw] " + p.first);
  return types;
}

} // namespace PDCommon::Contact
