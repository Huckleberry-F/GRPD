#ifndef MATERIALREGISTRY_H
#define MATERIALREGISTRY_H

// ============================================================================
// MaterialRegistry.h - 材料全局注册中心 (单例)
// 责任：
// 1. 保存所有编译期静态注入的材料工厂函数 (registryMap_)
// 2. 提供统一的接口用于创建具体的材料对象
// 架构对标：完全对称于 ModelDataRegistry 的工厂注册模式
// ============================================================================

#include "Material.h"
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace PDCommon::Material {

// ---------------------------------------------------------------------------
// 工厂函数定义：根据名称返回一个具体 Material 的 unique_ptr
// ---------------------------------------------------------------------------
typedef std::function<std::unique_ptr<Material>(const std::string &)>
    MaterialCreatorFunc;

class MaterialRegistry {
public:
  // -----------------------------------------------------------------------
  // 单例模式访问接口
  // -----------------------------------------------------------------------
  static MaterialRegistry &getInstance();

  // 禁用拷贝构造和赋值
  MaterialRegistry(const MaterialRegistry &) = delete;
  MaterialRegistry &operator=(const MaterialRegistry &) = delete;

  // -----------------------------------------------------------------------
  // 工厂注册机制 (Registry)
  // -----------------------------------------------------------------------
  /// @brief Register a material type with its creator function
  /// @param type    Material type name (e.g., "Elastic", "ThermalMat")
  /// @param creator Function to create the material instance
  void registerMaterialType(const std::string &type,
                            MaterialCreatorFunc creator);

  /// @brief Create a material instance using the registered creator function
  /// @param type Registered material type name
  /// @param name New instance name
  /// @return Created Material smart pointer
  std::unique_ptr<Material> createMaterial(const std::string &type,
                                           const std::string &name);

  /// @brief 获取所有已注册的类型名列表
  std::vector<std::string> getRegisteredTypes() const;

private:
  MaterialRegistry() = default;
  ~MaterialRegistry() = default;

  // Material type registry: type name -> creator function
  std::map<std::string, MaterialCreatorFunc> registryMap_;
};
// ---------------------------------------------------------------------------
// Macro definition for automatically registering specific material subclasses
// ---------------------------------------------------------------------------
#define REGISTER_MATERIAL_TYPE(Type, Creator)                                  \
  namespace {                                                                  \
  struct MaterialRegistrar_##Type {                                            \
    MaterialRegistrar_##Type() {                                               \
      PDCommon::Material::MaterialRegistry::getInstance().registerMaterialType(    \
          #Type, Creator);                                                     \
    }                                                                          \
  };                                                                           \
  static MaterialRegistrar_##Type global_MaterialRegistrar_##Type;             \
  }

} // namespace PDCommon::Material

#endif // PDCOMMON_MODEL_MATERIAL_REGISTRY_H
