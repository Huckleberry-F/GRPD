#ifndef PDCOMMON_FIELD_FIELD_MANAGER_H
#define PDCOMMON_FIELD_FIELD_MANAGER_H

// ============================================================================
// FieldManager.h - 物理场管理器 (纯容器)
// 责任：
// 1. 统一持有所有已创建的物理场实例
// 2. 提供按名称查询、批量操作的接口
// 架构规约：对标 MaterialManager — 只负责存储，不负责创建
// ============================================================================

#include "Field.h"
#include "TypedField.h"
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <memory>
#include <string>
#include <vector>

namespace PDCommon::Field {

class FieldManager {
public:
  FieldManager() = default;
  ~FieldManager() = default;

  // 禁用拷贝，允许移动
  FieldManager(const FieldManager &) = delete;
  FieldManager &operator=(const FieldManager &) = delete;
  FieldManager(FieldManager &&) = default;
  FieldManager &operator=(FieldManager &&) = default;

  // -----------------------------------------------------------------------
  // 实例添加接口（对标 MaterialManager::addMaterial）
  // -----------------------------------------------------------------------

  /// @brief 添加一个已创建的物理场实例（幂等：同名场已存在则直接返回已有指针）
  /// @param field 场实例的所有权（unique_ptr），名称自动从 Field::getName() 提取
  /// @return 场指针（管理器持有所有权）
  Field *addField(std::unique_ptr<Field> field);

  // -----------------------------------------------------------------------
  // 查询接口
  // -----------------------------------------------------------------------

  /// @brief 获取已注册的物理场基类指针（不存在返回 nullptr）
  Field *getField(const std::string &name);
  const Field *getField(const std::string &name) const;

  /// @brief 获取带类型安全的物理场指针（不存在或类型不匹配返回 nullptr）
  template <typename T = double>
  TypedField<T> *getFieldAs(const std::string &name);
  template <typename T = double>
  const TypedField<T> *getFieldAs(const std::string &name) const;

  /// @brief 检查某个物理场是否已注册
  bool hasField(const std::string &name) const;

  /// @brief 获取所有已注册物理场的名称列表
  std::vector<std::string> getFieldNames() const;

  /// @brief 获取已注册物理场总数
  size_t getFieldCount() const;

  // -----------------------------------------------------------------------
  // 批量操作
  // -----------------------------------------------------------------------

  /// @brief 对所有已注册的场统一按粒子数分配内存
  void resizeAll(size_t numParticles);

  // -----------------------------------------------------------------------
  // 全局 Swap 注册与执行机制 (DoD架构防踩踏设计)
  // -----------------------------------------------------------------------

  /// @brief 声明一对需要在回合末执行指针 O(1) swap 的场
  /// 使用 Set 自动去重，免疫多材料重复注册
  void registerSwapPair(const std::string &oldField, const std::string &trialField);

  /// @brief 一次性遍历执行所有已注册的情侣场互换
  void executeAllRegisteredSwaps();

private:
  /// 物理场容器：场名 -> unique_ptr<Field>
  std::map<std::string, std::unique_ptr<Field>> fields_;

  /// 待互换场的黑名单集合：用于回合末的清理
  std::set<std::pair<std::string, std::string>> swapPairs_;
};

// ===========================================================================
// 模板方法内联实现
// ===========================================================================

template <typename T>
TypedField<T> *FieldManager::getFieldAs(const std::string &name) {
  auto it = fields_.find(name);
  if (it == fields_.end()) {
    return nullptr;
  }
  return dynamic_cast<TypedField<T> *>(it->second.get());
}

template <typename T>
const TypedField<T> *FieldManager::getFieldAs(const std::string &name) const {
  auto it = fields_.find(name);
  if (it == fields_.end()) {
    return nullptr;
  }
  return dynamic_cast<const TypedField<T> *>(it->second.get());
}

} // namespace PDCommon::Field

#endif // PDCOMMON_FIELD_FIELD_MANAGER_H
