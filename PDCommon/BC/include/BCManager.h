#ifndef PDCOMMON_BC_BC_MANAGER_H
#define PDCOMMON_BC_BC_MANAGER_H

// ============================================================================
// BCManager.h - 边界条件实例管理器（纯容器，对标 MaterialManager）
// 责任：
// 1. 管理某个具体模型上下文中的所有边界条件实例
// 2. 提供增/删/查/遍历的纯容器接口
// 3. 不包含任何施加/计算逻辑——调度由 BC 静态方法提供
// ============================================================================

#include "BC.h"
#include <memory>
#include <vector>

namespace PDCommon::BC {

class BCManager {
public:
  BCManager() = default;
  ~BCManager() = default;

  // 禁用拷贝，允许移动
  BCManager(const BCManager &) = delete;
  BCManager &operator=(const BCManager &) = delete;
  BCManager(BCManager &&) = default;
  BCManager &operator=(BCManager &&) = default;

  // -----------------------------------------------------------------------
  // 边界条件实例管理（纯容器接口）
  // -----------------------------------------------------------------------

  /// @brief 添加一个边界条件实例（独占所有权）
  /// @param bc 边界条件实例的 unique_ptr
  /// @param step 隶属的载荷步 (0 为全局)
  void addBC(std::unique_ptr<BC> bc, int step = 0);

  /// @brief 获取当前管理的边界条件总数
  size_t size() const;

  /// @brief 清空所有边界条件
  void clear();

  /// @brief 获取所有 BC 条目的可写引用（供调度函数遍历）
  std::vector<BCEntry> &getBCs() { return bcs_; }

  /// @brief 获取所有 BC 条目的只读引用
  const std::vector<BCEntry> &getBCs() const { return bcs_; }

private:
  // 统一容器，存储附带 Step ID 的 BC
  std::vector<BCEntry> bcs_;
};

} // namespace PDCommon::BC

#endif // PDCOMMON_BC_BC_MANAGER_H
