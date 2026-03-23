// ============================================================================
// MaterialManager.cpp - 材料实例管理器实现
// ============================================================================

#include "MaterialManager.h"
#include "Logger.h"
#include <stdexcept>

namespace PDCommon::Material {

// ---------------------------------------------------------------------------
// 添加材料实例
// ---------------------------------------------------------------------------
void MaterialManager::addMaterial(const std::string &name,
                                  std::unique_ptr<Material> material) {
  if (materials_.find(name) != materials_.end()) {
    LOG_WARNING("[MaterialManager] Material '" + name +
                "' has been exist, overwriting.");
  }
  materials_[name] = std::move(material);
  LOG_INFO("[MaterialManager] Successfully added material instance: " + name);
}

// ---------------------------------------------------------------------------
// 获取材料实例
// ---------------------------------------------------------------------------
Material *MaterialManager::getMaterial(const std::string &name) {
  auto it = materials_.find(name);
  if (it == materials_.end()) {
    LOG_ERROR("[MaterialManager] Failed to find material instance: " + name);
    throw std::runtime_error("Not find material instance" + name);
  }
  return it->second.get();
}

const Material *MaterialManager::getMaterial(const std::string &name) const {
  auto it = materials_.find(name);
  if (it == materials_.end()) {
    LOG_ERROR("[MaterialManager] Failed to find material instance: " + name);
    throw std::runtime_error("Not find material instance: " + name);
  }
  return it->second.get();
}

// ---------------------------------------------------------------------------
// 检查材料实例是否存在
// ---------------------------------------------------------------------------
bool MaterialManager::hasMaterial(const std::string &name) const {
  return materials_.find(name) != materials_.end();
}

} // namespace PDCommon::Material
