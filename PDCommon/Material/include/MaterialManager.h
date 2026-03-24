#ifndef PDCOMMON_MATERIAL_MANAGER_H
#define PDCOMMON_MATERIAL_MANAGER_H

// ============================================================================
// MaterialManager.h - 材料实例管理器 (非单例，每个模型一个)
// 责任：管理某个具体模型上下文中的所有材料实例。
// 架构对标：完全对称于 ParticleManager（非单例，由上层 Registry 持有）
// ============================================================================

#include "Material.h"
#include <map>
#include <memory>
#include <string>

namespace PDCommon::Material {

class MaterialManager {
public:
  MaterialManager() = default; // 公有构造函数，由 MaterialRegistry 创建
  ~MaterialManager() = default;

  // 禁用拷贝构造和赋值
  MaterialManager(const MaterialManager &) = delete;
  MaterialManager &operator=(const MaterialManager &) = delete;

  // 允许移动构造和赋值
  MaterialManager(MaterialManager &&) = default;
  MaterialManager &operator=(MaterialManager &&) = default;

  // -----------------------------------------------------------------------
  // 材料实例管理
  // -----------------------------------------------------------------------

  /// @brief 添加一个已创建好的材料实例到当前管理器中
  /// @param material 材料实例的所有权（名称自动从 Material::getName() 提取）
  void addMaterial(std::unique_ptr<Material> material);

  /// @brief 获取已实例化的材料对象指针
  /// @param name 材料实例名称
  Material *getMaterial(const std::string &name);
  const Material *getMaterial(const std::string &name) const;

  /// @brief 检查材料实例是否存在
  bool hasMaterial(const std::string &name) const;

  /// @brief 获取已注册材料总数
  size_t getMaterialCount() const { return materials_.size(); }

  /// @brief 获取所有材料的引用（用于遍历，如分配状态变量）
  const std::map<std::string, std::unique_ptr<Material>> &getMaterials() const {
    return materials_;
  }

private:
  // 存储当前模型上下文中的所有材料实例（实例名 -> unique_ptr<Material>）
  std::map<std::string, std::unique_ptr<Material>> materials_;
};

} // namespace PDCommon::Material

#endif // PDCOMMON_MODEL_MATERIAL_MANAGER_H
