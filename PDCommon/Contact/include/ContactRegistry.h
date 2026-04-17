#ifndef PDCOMMON_CONTACT_CONTACTREGISTRY_H
#define PDCOMMON_CONTACT_CONTACTREGISTRY_H

#include "IContactAlgorithm.h"
#include "IContactForceLaw.h"
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace PDCommon::Contact {

// =========================================================================
// 工厂函数类型别名
// =========================================================================

/// @brief ForceLaw 创建函数类型
typedef std::function<std::unique_ptr<IContactForceLaw>()> ForceLawCreatorFunc;

/// @brief 扁平化接触算法创建函数类型
/// @param name 接触对实例名称
/// @param forceLaw 力学本构对象指针（对于 Kinematic 此指针为空，被忽略）
typedef std::function<std::unique_ptr<IContactAlgorithm>(
    const std::string &name, std::unique_ptr<IContactForceLaw> forceLaw)>
    ContactCreatorFunc;

// =========================================================================
// ContactRegistry - 核心注册中心
// =========================================================================
class ContactRegistry {
public:
  static ContactRegistry &getInstance();

  ContactRegistry(const ContactRegistry &) = delete;
  ContactRegistry &operator=(const ContactRegistry &) = delete;

  /// @brief 注册力学定律类型（如 "Penalty" → PenaltyForceLaw）
  void registerForceLawType(const std::string &type,
                            ForceLawCreatorFunc creator);

  /// @brief 注册顶层接触算法（如 "NTN", "Kinematic"）
  void registerContactType(const std::string &type,
                           ContactCreatorFunc creator);

  /// @brief 工厂方法：创建接触算法实例
  /// @param type YAML Type 字段（"NTN" / "Kinematic" 等）
  /// @param name 接触对实例名称
  /// @param forceLawType YAML ForceLaw 字段（仅当 Type 需要时有效）
  std::unique_ptr<IContactAlgorithm>
  createContact(const std::string &type, const std::string &name,
                const std::string &forceLawType = "Penalty");

  std::vector<std::string> getRegisteredTypes() const;

private:
  ContactRegistry() = default;
  ~ContactRegistry() = default;

  std::map<std::string, ForceLawCreatorFunc> forceLawMap_;
  std::map<std::string, ContactCreatorFunc> contactMap_;
};

// =========================================================================
// 注册宏：力学定律
// 用法：REGISTER_FORCELAW_TYPE("Penalty", PenaltyForceLaw)
// =========================================================================
#define REGISTER_FORCELAW_TYPE(TypeString, ForceLawClass)                       \
  namespace {                                                                  \
  struct ForceLawRegistrar_##ForceLawClass {                                   \
    ForceLawRegistrar_##ForceLawClass() {                                      \
      PDCommon::Contact::ContactRegistry::getInstance().registerForceLawType(  \
          TypeString, []() {                                                   \
            return std::make_unique<PDCommon::Contact::ForceLawClass>();        \
          });                                                                  \
    }                                                                          \
  };                                                                           \
  static ForceLawRegistrar_##ForceLawClass                                     \
      global_ForceLawRegistrar_##ForceLawClass;                                \
  }

// =========================================================================
// 注册宏：顶层接触算法（Type 字段）
// =========================================================================
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
