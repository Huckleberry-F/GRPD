#ifndef PDCOMMON_CONTACT_CONTACTREGISTRY_H
#define PDCOMMON_CONTACT_CONTACTREGISTRY_H

#include "IContactAlgorithm.h"
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace PDCommon::Contact {

typedef std::function<std::unique_ptr<IContactAlgorithm>(const std::string &)>
    ContactCreatorFunc;

class ContactRegistry {
public:
  static ContactRegistry &getInstance();

  ContactRegistry(const ContactRegistry &) = delete;
  ContactRegistry &operator=(const ContactRegistry &) = delete;

  void registerContactType(const std::string &type, ContactCreatorFunc creator);

  std::unique_ptr<IContactAlgorithm> createContact(const std::string &type,
                                                   const std::string &name);

  std::vector<std::string> getRegisteredTypes() const;

private:
  ContactRegistry() = default;
  ~ContactRegistry() = default;

  std::map<std::string, ContactCreatorFunc> registryMap_;
};

#define REGISTER_CONTACT_TYPE(Type, Creator)                                   \
  namespace {                                                                  \
  struct ContactRegistrar_##Type {                                             \
    ContactRegistrar_##Type() {                                                \
      PDCommon::Contact::ContactRegistry::getInstance().registerContactType(   \
          #Type, Creator);                                                     \
    }                                                                          \
  };                                                                           \
  static ContactRegistrar_##Type global_ContactRegistrar_##Type;               \
  }

} // namespace PDCommon::Contact

#endif // PDCOMMON_CONTACT_CONTACTREGISTRY_H
