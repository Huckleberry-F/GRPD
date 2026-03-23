// ============================================================================
// NeighborList.h - CSR 格式的高性能邻域搜索管理器
// 数据布局：
//   offsets_[N+1]          — CSR 行指针
//   neighborIds_[总键数]    — 扁平邻居 ID 连续数组
//   bondLengths_[总键数]    — 扁平初始键长连续数组
//   bondFields_[name]      — 动态注册的 per-bond 状态变量（如 damage、stretch）
// 优势：连续内存，零锁竞争并行填充，键场共享 CSR 偏移量
// ============================================================================

#ifndef PDCOMMON_NEIGHBOR_NEIGHBOR_LIST_H
#define PDCOMMON_NEIGHBOR_NEIGHBOR_LIST_H

#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

namespace PDCommon::Model {
class ParticleManager;
}

namespace PDCommon::Neighbor {

class NeighborList {
public:
  NeighborList() = default;
  ~NeighborList() = default;

  // 禁用拷贝
  NeighborList(const NeighborList &) = delete;
  NeighborList &operator=(const NeighborList &) = delete;

  /// @brief 使用两步法 Cell List 算法构建 CSR 格式近邻列表
  /// @param mgr ParticleManager 实例，包含所有粒子位置
  /// @param horizon 邻域半径 delta
  void buildNeighbors(const PDCommon::Model::ParticleManager &mgr,
                      double horizon);

  // -----------------------------------------------------------------------
  // CSR 格式访问接口
  // -----------------------------------------------------------------------

  /// @brief 获取粒子 i 的邻居数量
  int getNeighborCount(int i) const {
    return offsets_[i + 1] - offsets_[i];
  }

  /// @brief 获取粒子 i 的邻居 ID 连续数组指针
  const int *getNeighborIds(int i) const {
    return &neighborIds_[offsets_[i]];
  }

  /// @brief 获取粒子 i 的键长连续数组指针
  const double *getBondLengths(int i) const {
    return &bondLengths_[offsets_[i]];
  }

  /// @brief 获取总粒子数
  size_t size() const { return offsets_.empty() ? 0 : offsets_.size() - 1; }

  /// @brief 获取总键数
  size_t totalBonds() const { return neighborIds_.size(); }

  /// @brief 获取邻域半径
  double getHorizon() const { return horizon_; }

  /// @brief 清空数据
  void clear();

  // -----------------------------------------------------------------------
  // BondField 动态注册接口（per-bond 状态变量）
  // -----------------------------------------------------------------------

  /// @brief 注册一个 per-bond 状态变量场（大小 = totalBonds，初始化为 0）
  void registerBondField(const std::string &name);

  /// @brief 获取键场裸指针（用于高性能遍历）
  double *getBondFieldPtr(const std::string &name);
  const double *getBondFieldPtr(const std::string &name) const;

  /// @brief 查询键场是否已注册
  bool hasBondField(const std::string &name) const;

  /// @brief 获取粒子 i 的键场数据指针（从 offsets_[i] 开始，长度 = getNeighborCount(i)）
  double *getBondFieldForParticle(const std::string &name, int i);
  const double *getBondFieldForParticle(const std::string &name, int i) const;

private:
  std::vector<int> offsets_;       ///< CSR 行指针，大小 N+1
  std::vector<int> neighborIds_;   ///< 扁平邻居 ID 数组
  std::vector<double> bondLengths_;///< 扁平初始键长数组
  double horizon_{0.0};

  /// 动态注册的 per-bond 状态变量（如 "damage"、"stretch"）
  std::unordered_map<std::string, std::vector<double>> bondFields_;
};

} // namespace PDCommon::Neighbor

#endif // PDCOMMON_NEIGHBOR_NEIGHBOR_LIST_H
