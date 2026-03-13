#ifndef PDSIMULATER_H
#define PDSIMULATER_H

// ============================================================================
// PDSimulater.h - 近场动力学仿真大管家 (核心枢纽)
// 责任：
// 1. 代表一个完整的 PD 物理或计算工程上下文
// 2. 独立持有自己专属的 ParticleManager、MaterialManager、FieldManager 等数据组件
// 3. 作为 PD 系统与外界 (如主程序、或者未来的 FEM 系统) 耦合的唯一数据桥梁
// ============================================================================

#include "MaterialManager.h"
#include "ParticleManager.h"
#include "FieldManager.h"
#include "NeighborList.h"
#include "BCManager.h"
#include <memory>
#include <string>

namespace GRPD::Core {

class PDSimulater {
public:
  /// @brief 构造函数，初始化一个全新的仿真模型上下文
  /// @param name 模型名称，用于标识和日志输出
  explicit PDSimulater(const std::string &name);

  ~PDSimulater() = default;

  // 禁用拷贝，允许移动（保持资源独占）
  PDSimulater(const PDSimulater &) = delete;
  PDSimulater &operator=(const PDSimulater &) = delete;
  PDSimulater(PDSimulater &&) = default;
  PDSimulater &operator=(PDSimulater &&) = default;

  /// @brief 获取模型名称
  const std::string &getName() const { return name_; }

  /// @brief 设置模型名称
  void setName(const std::string &name) { name_ = name; }

  /// @brief 获取粒子管理器引用（非 const，用于求解和初始化）
  GRPD::Model::ParticleManager &getParticleManager() {
    return particleManager_;
  }
  const GRPD::Model::ParticleManager &getParticleManager() const {
    return particleManager_;
  }

  /// @brief 获取材料管理器引用（非 const，用于绑定和求解）
  GRPD::Material::MaterialManager &getMaterialManager() {
    return materialManager_;
  }
  const GRPD::Material::MaterialManager &getMaterialManager() const {
    return materialManager_;
  }

  // -----------------------------------------------------------------------
  // 物理场管理器 (FieldManager) — 统一管理所有物理场
  // -----------------------------------------------------------------------

  /// @brief 获取物理场管理器引用
  GRPD::Field::FieldManager &getFieldManager() { return fieldManager_; }
  const GRPD::Field::FieldManager &getFieldManager() const {
    return fieldManager_;
  }

  // -----------------------------------------------------------------------
  // 近邻列表 (NeighborList) 按需分配
  // -----------------------------------------------------------------------
  void createNeighborList(const GRPD::Model::ParticleManager &mgr,
                         double horizon);
  bool hasNeighborList() const { return neighborList_ != nullptr; }
  GRPD::Initial::NeighborList &getNeighborList() { return *neighborList_; }
  const GRPD::Initial::NeighborList &getNeighborList() const {
    return *neighborList_;
  }

  // -----------------------------------------------------------------------
  // 边界条件管理器 (BCManager)
  // -----------------------------------------------------------------------
  GRPD::BC::BCManager &getBCManager() { return bcManager_; }
  const GRPD::BC::BCManager &getBCManager() const { return bcManager_; }

private:
  std::string name_;                             // 模型名称
  GRPD::Model::ParticleManager particleManager_; // 该模型专属的粒子数组与管理器
  GRPD::Material::MaterialManager materialManager_; // 该模型专属的材料实例管理
  GRPD::Field::FieldManager fieldManager_;       // 物理场管理器 (统一管理所有场数据)

  std::unique_ptr<GRPD::Initial::NeighborList>
      neighborList_; // 近邻列表（按需分配）

  GRPD::BC::BCManager bcManager_; // 边界条件管理器 (静态分配)
};

} // namespace GRPD::Core

#endif // PDSIMULATER_H
