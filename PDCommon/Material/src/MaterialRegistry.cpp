// ============================================================================
// MaterialRegistry.cpp - 材料全局注册中心实现
// ============================================================================

#include "MaterialRegistry.h"
#include "Logger.h"
#include <stdexcept>

namespace GRPD::Material {

// ---------------------------------------------------------------------------
// 单例实例获取
// ---------------------------------------------------------------------------
MaterialRegistry &MaterialRegistry::getInstance() {
  static MaterialRegistry instance;
  return instance;
}

// ---------------------------------------------------------------------------
// 注册材料工厂函数
// ---------------------------------------------------------------------------
void MaterialRegistry::registerMaterialType(const std::string &type,
                                            MaterialCreatorFunc creator) {
  if (registryMap_.find(type) != registryMap_.end()) {
    LOG_WARNING("[MaterialRegistry] Material type '" + type +
                "' has been registry, overwrite it.");
  } else {
    LOG_INFO("[MaterialRegistry] Material type '" + type +
             "' has been registry.");
  }
  registryMap_[type] = creator;
}

// ---------------------------------------------------------------------------
// 通过工厂创建材料实例（不保管，返回所有权）
// ---------------------------------------------------------------------------
std::unique_ptr<Material>
MaterialRegistry::createMaterial(const std::string &type,
                                 const std::string &name) {
  auto it = registryMap_.find(type);
  if (it == registryMap_.end()) {
    LOG_ERROR(
        "[MaterialRegistry] Create material failed: unknown material type '" +
        type + "' !");
    throw std::runtime_error("Unknown material type: " + type);
  }
  LOG_INFO("[MaterialRegistry] Using factory to create material: " + name +
           " (type: " + type + ")");
  return it->second(name);
}

} // namespace GRPD::Material
