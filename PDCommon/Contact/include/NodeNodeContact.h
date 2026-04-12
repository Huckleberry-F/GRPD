#ifndef PDCOMMON_CONTACT_NODENODECONTACT_H
#define PDCOMMON_CONTACT_NODENODECONTACT_H

// ============================================================================
// NodeNodeContact.h - 点对点接触算法的公共中间基类（模板方法模式）
// 责任：
//   1. 封装空间哈希网格 (Spatial Hash Grid) 的构建与搜索
//   2. 实现 computeContactForce 的完整骨架：
//      场提取 → 网格构建 → OMP碰撞对遍历 → 初始邻居过滤 → 力注入
//   3. 子类只需实现 computePairForce() 来定义碰撞对的力计算逻辑
// ============================================================================

#include "IContactAlgorithm.h"
#include <Eigen/Dense>
#include <vector>

namespace PDCommon::Contact {

/// @brief 单个碰撞对 (i, j) 的上下文信息
struct ContactPairContext {
  int i, j;                   ///< slave 和 master 粒子索引
  double dist;                ///< 当前距离
  double safeDist;            ///< 安全距离 (dx_i + dx_j) / 2
  double raw_penetration;     ///< safeDist - dist（侵入量）
  double nx, ny, nz;          ///< 法向单位矢量（j→i 方向）
  double mass_i, mass_j;      ///< slave 和 master 质量
  double dx_i, dx_j;          ///< 等效边长
  const double *vel;          ///< 速度场指针（可为 nullptr）
};

/// @brief 碰撞对力学计算结果
struct ContactPairResult {
  double fx = 0.0, fy = 0.0, fz = 0.0; ///< 施加在 slave i 上的力（反作用力自动注入 master j）
  bool valid = true;                     ///< 如果为 false，跳过本对
};

class NodeNodeContact : public IContactAlgorithm {
public:
  explicit NodeNodeContact(const std::string &name = "")
      : IContactAlgorithm(name) {}
  ~NodeNodeContact() override = default;

  /// @brief 模板方法：完整的接触力计算骨架（子类不需要重写）
  void computeContactForce(PDCommon::Core::PDContext &ctx) override;

protected:
  // -----------------------------------------------------------------------
  // 空间哈希网格状态（供所有子类共享）
  // -----------------------------------------------------------------------
  double cellSize_ = 0.0;
  Eigen::Vector3d minBounds_;
  Eigen::Vector3d maxBounds_;
  Eigen::Vector3i gridDims_;
  std::vector<int> head_;
  std::vector<int> next_;

  // -----------------------------------------------------------------------
  // 公用空间搜索方法
  // -----------------------------------------------------------------------
  void buildCellList(const double *coords, const double *disp,
                     const int *activeStatus, double maxDx);
  int computeCellHash(double x, double y, double z) const;

  // -----------------------------------------------------------------------
  // 子类钩子函数
  // -----------------------------------------------------------------------

  /// @brief 预处理钩子（在网格构建前调用）
  /// @details 子类可在此估算刚度等参数。默认为空实现。
  virtual void onPreContact(PDCommon::Core::PDContext &ctx, double maxDx) {}

  /// @brief 计算单个碰撞对的力学结果（纯虚函数，子类必须实现）
  /// @param pair 碰撞对上下文（含位置、距离、法向、质量等信息）
  /// @return ContactPairResult 力向量（基类负责转换为加速度并注入）
  virtual ContactPairResult computePairForce(const ContactPairContext &pair) = 0;
};

} // namespace PDCommon::Contact

#endif // PDCOMMON_CONTACT_NODENODECONTACT_H
