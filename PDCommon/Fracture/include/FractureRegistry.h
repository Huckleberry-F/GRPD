// ============================================================================
// FractureRegistry.h - 损伤工厂
// ============================================================================

#ifndef PDCOMMON_FRACTURE_DAMAGEREGISTRY_H
#define PDCOMMON_FRACTURE_DAMAGEREGISTRY_H

#include "FractureModel.h"
#include <functional>
#include <map>
#include <memory>
#include <string>

namespace PDCommon::Fracture {

/// @brief 损伤模型注册表/单例工厂
class FractureRegistry {
public:
  using Creator = std::function<std::unique_ptr<FractureModel>()>;

  static FractureRegistry &getInstance();

  /// @brief 注册构造函数
  void registerFactory(const std::string &name, Creator creator);

  /// @brief 创建损伤模型实例
  std::unique_ptr<FractureModel> create(const std::string &name) const;

  /// @brief 是否存在指定的损伤模型
  bool contains(const std::string &name) const;

private:
  FractureRegistry() = default;
  std::map<std::string, Creator> factories_;
};

/// @brief 用于在全局域静态注册损伤模型的辅助类
class FractureRegistrar {
public:
  FractureRegistrar(const std::string &name, FractureRegistry::Creator creator) {
    FractureRegistry::getInstance().registerFactory(name, std::move(creator));
  }
};

#define REGISTER_FRACTURE_MODEL(Name, Class)                                     \
  static const PDCommon::Fracture::FractureRegistrar registrar_Fracture_##Name(      \
      #Name, []() { return std::make_unique<Class>(); });

} // namespace PDCommon::Fracture

#endif // PDCOMMON_FRACTURE_DAMAGEREGISTRY_H
