#ifndef PDCOMMON_CONTACT_NODESURFACECONTACT_H
#define PDCOMMON_CONTACT_NODESURFACECONTACT_H

// ============================================================================
// NodeSurfaceContact.h - 点到面 (Node-to-Surface) 接触算法的公共中间基类
// 责任：
//   1. 借助 ContactSpatialGrid 执行空间近邻搜索
//   2. 对每个 slave 粒子，将周围的 master 粒子群通过加权平均融合为
//      一个"虚拟等效面"（Phantom Surface），生成平滑的法向量和等效质量
//   3. 将融合后的面上下文传递给子类的 computeSurfaceForce()
//   4. 按照权重进行反作用力的反向分配，保证牛顿第三定律
//   5. 子类只需实现受力公式，无需关心搜索与分配逻辑
// ============================================================================

#include "ContactSpatialGrid.h"
#include "IContactAlgorithm.h"
#include <utility>
#include <vector>

namespace PDCommon::Contact {

/// @brief 融合后的"虚拟等效面"上下文信息
/// @details 由基类在搜索与加权平均后生成，传递给子类的 computeSurfaceForce()
struct SurfaceContext {
  int slave_id;                               ///< 从面粒子索引
  double mass_slave;                          ///< 从面粒子质量
  double maxPenetration;                      ///< 最大侵入深度
  double nx, ny, nz;                          ///< 加权平均后的平滑法向单位矢量
  double vx_master, vy_master, vz_master;     ///< 加权平均后的等效主面速度
  double m_master_eff;                        ///< 等效主面有效质量
  const double *vel;                          ///< 全局速度场指针
};

/// @brief 面接触力计算结果
struct SurfaceForceResult {
  double fx = 0.0, fy = 0.0, fz = 0.0;       ///< 施加在 slave 上的合力
  double ft_x = 0.0, ft_y = 0.0, ft_z = 0.0; ///< 切向摩擦力分量（预留）
  bool valid = true;
};

class NodeSurfaceContact : public IContactAlgorithm {
public:
  explicit NodeSurfaceContact(const std::string &name = "")
      : IContactAlgorithm(name) {}
  ~NodeSurfaceContact() override = default;

  /// @brief 完整的 Node-to-Surface 接触力计算骨架
  /// @details 执行：场提取 → 网格构建 → 加权融合 → 子类受力 → 反向分配
  void computeContactForce(PDCommon::Core::PDContext &ctx) override;

protected:
  // -----------------------------------------------------------------------
  // 空间哈希网格（组合方式）
  // -----------------------------------------------------------------------
  ContactSpatialGrid grid_;
  bool checkInitialNeighbor_ = true; ///< 是否过滤初始键，默认开启


  // -----------------------------------------------------------------------
  // 子类钩子函数
  // -----------------------------------------------------------------------

  /// @brief 预处理钩子（在网格构建前调用）
  /// @details 子类可在此读取额外参数。默认空实现。
  virtual void onPreContact(PDCommon::Core::PDContext &ctx, double maxDx) {}

  /// @brief 计算面接触力（纯虚函数，子类必须实现）
  /// @param surf 融合后的虚拟面上下文（含平滑法向、等效速度、等效质量等）
  /// @return SurfaceForceResult 施加在 slave 上的法向（和切向）力
  virtual SurfaceForceResult
  computeSurfaceForce(const SurfaceContext &surf) = 0;
};

} // namespace PDCommon::Contact

#endif // PDCOMMON_CONTACT_NODESURFACECONTACT_H
