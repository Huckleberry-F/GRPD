#ifndef PDCOMMON_CONTACT_CONTACTMANAGER_H
#define PDCOMMON_CONTACT_CONTACTMANAGER_H

// ============================================================================
// ContactManager.h - 接触对管理器（纯容器，对标 MaterialManager）
// 责任：
// 1. 统一持有并管理所有接触对（Contact Pair）实例
// 2. 提供增/删/查/遍历的纯容器接口
// 3. 不包含任何计算逻辑——力计算调度由 TimeIntegrator 负责
// ============================================================================

#include "IContactAlgorithm.h"
#include <memory>
#include <string>
#include <unordered_map>

namespace PDCommon::Contact {

class ContactManager {
public:
  ContactManager() = default;
  ~ContactManager() = default;

  // 禁用拷贝，允许移动
  ContactManager(const ContactManager &) = delete;
  ContactManager &operator=(const ContactManager &) = delete;
  ContactManager(ContactManager &&) = default;
  ContactManager &operator=(ContactManager &&) = default;

  // -----------------------------------------------------------------------
  // 接触对实例管理（纯容器接口，对标 MaterialManager）
  // -----------------------------------------------------------------------

  /// @brief 添加一个已构造好的接触对实例（所有权转移）
  void addContactPair(std::unique_ptr<IContactAlgorithm> contact);

  /// @brief 按名称查找接触对（不转移所有权）
  IContactAlgorithm *getContactPair(const std::string &name);
  const IContactAlgorithm *getContactPair(const std::string &name) const;

  /// @brief 检查接触对是否存在
  bool hasContactPair(const std::string &name) const;

  /// @brief 获取接触对总数
  size_t getContactPairCount() const { return contactPairs_.size(); }

  /// @brief 获取所有接触对的只读引用（用于外部遍历）
  const std::unordered_map<std::string, std::unique_ptr<IContactAlgorithm>> &
  getContactPairs() const { return contactPairs_; }

  /// @brief 获取所有接触对的可写引用（供 TimeIntegrator 遍历并调用计算）
  std::unordered_map<std::string, std::unique_ptr<IContactAlgorithm>> &
  getContactPairs() { return contactPairs_; }

  /// @brief 清空所有接触对
  void clear();

private:
  /// 所有接触对实例，按名称索引
  std::unordered_map<std::string, std::unique_ptr<IContactAlgorithm>> contactPairs_;
};

} // namespace PDCommon::Contact

#endif // PDCOMMON_CONTACT_CONTACTMANAGER_H
