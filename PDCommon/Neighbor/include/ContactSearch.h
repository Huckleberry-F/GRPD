// ============================================================================
// ContactSearch.h - 接触搜索抽象接口
// 责任：
// 1. 定义接触检测的统一接口
// 2. 具体实现（如节点-面、面-面）由派生类完成
// 注意：当前为预留接口，尚未实现具体算法
// ============================================================================

#ifndef PDCOMMON_NEIGHBOR_CONTACT_SEARCH_H
#define PDCOMMON_NEIGHBOR_CONTACT_SEARCH_H

#include <vector>
#include <cstddef>

namespace PDCommon::Model {
class ParticleManager;
}

namespace PDCommon::Neighbor {

/// @brief 接触对信息
struct ContactPair {
  int masterId;  ///< 主面/节点 ID
  int slaveId;   ///< 从面/节点 ID
  double gap;    ///< 法向间隙（负值表示穿透）
  double normal[3]; ///< 接触法向量
};

/// @brief 接触搜索抽象基类
/// @details 定义接触检测的统一接口。
///          具体实现（如节点-面、面-面）由派生类完成。
///          当前为预留接口，尚未实现具体算法。
class ContactSearch {
public:
  ContactSearch() = default;
  virtual ~ContactSearch() = default;

  // 禁用拷贝
  ContactSearch(const ContactSearch &) = delete;
  ContactSearch &operator=(const ContactSearch &) = delete;

  /// @brief 执行接触搜索，更新接触对列表
  /// @param mgr 粒子管理器，提供几何数据
  virtual void detectContacts(
      const PDCommon::Model::ParticleManager &mgr) = 0;

  /// @brief 获取当前检测到的所有接触对
  virtual const std::vector<ContactPair> &getContactPairs() const = 0;

  /// @brief 获取接触对数量
  virtual size_t getContactCount() const = 0;

  /// @brief 清空接触对列表
  virtual void clear() = 0;
};

} // namespace PDCommon::Neighbor

#endif // PDCOMMON_NEIGHBOR_CONTACT_SEARCH_H
