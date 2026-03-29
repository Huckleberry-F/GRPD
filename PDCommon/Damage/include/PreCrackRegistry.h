#ifndef PDCOMMON_DAMAGE_PRECRACKREGISTRY_H
#define PDCOMMON_DAMAGE_PRECRACKREGISTRY_H

#include "PreCrackModel.h"
#include <functional>
#include <map>
#include <memory>
#include <string>

namespace PDCommon::Damage {

class PreCrackRegistry {
public:
  using Creator = std::function<std::unique_ptr<PreCrackModel>()>;

  static PreCrackRegistry &getInstance() {
    static PreCrackRegistry instance;
    return instance;
  }

  void registerType(const std::string &type, Creator creator) {
    registry_[type] = std::move(creator);
  }

  std::unique_ptr<PreCrackModel> create(const std::string &type) {
    auto it = registry_.find(type);
    if (it != registry_.end()) {
      auto model = it->second();
      model->setType(type);
      return model;
    }
    return nullptr;
  }

  bool contains(const std::string &type) const {
    return registry_.find(type) != registry_.end();
  }

private:
  PreCrackRegistry() = default;
  std::map<std::string, Creator> registry_;
};

} // namespace PDCommon::Damage

#define REGISTER_PRECRACK_MODEL(Type, Class)                                   \
  namespace {                                                                  \
  struct PreCrackRegister##Type {                                              \
    PreCrackRegister##Type() {                                                 \
      PDCommon::Damage::PreCrackRegistry::getInstance().registerType(          \
          #Type, []() { return std::make_unique<Class>(); });                  \
    }                                                                          \
  };                                                                           \
  static PreCrackRegister##Type global_PreCrackRegister##Type;                 \
  }

#endif // PDCOMMON_DAMAGE_PRECRACKREGISTRY_H
