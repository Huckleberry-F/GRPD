// ============================================================================
// DamageRegistry.h - 损伤工厂
// ============================================================================

#ifndef PDCOMMON_DAMAGE_DAMAGEREGISTRY_H
#define PDCOMMON_DAMAGE_DAMAGEREGISTRY_H

#include "DamageModel.h"
#include <functional>
#include <map>
#include <memory>
#include <string>

namespace PDCommon::Damage {

/// @brief 损伤模型注册表/单例工厂
class DamageRegistry {
public:
  using Creator = std::function<std::unique_ptr<DamageModel>()>;

  static DamageRegistry &getInstance();

  /// @brief 注册构造函数
  void registerFactory(const std::string &name, Creator creator);

  /// @brief 创建损伤模型实例
  std::unique_ptr<DamageModel> create(const std::string &name) const;

  /// @brief 是否存在指定的损伤模型
  bool contains(const std::string &name) const;

private:
  DamageRegistry() = default;
  std::map<std::string, Creator> factories_;
};

/// @brief 用于在全局域静态注册损伤模型的辅助类
class DamageRegistrar {
public:
  DamageRegistrar(const std::string &name, DamageRegistry::Creator creator) {
    DamageRegistry::getInstance().registerFactory(name, std::move(creator));
  }
};

#define REGISTER_DAMAGE_MODEL(Name, Class)                                     \
  static const PDCommon::Damage::DamageRegistrar registrar_Damage_##Name(      \
      #Name, []() { return std::make_unique<Class>(); });

} // namespace PDCommon::Damage

#endif // PDCOMMON_DAMAGE_DAMAGEREGISTRY_H
