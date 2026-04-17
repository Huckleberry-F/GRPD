#ifndef PDCOMMON_CONTACT_KINEMATICCONTACT_H
#define PDCOMMON_CONTACT_KINEMATICCONTACT_H

// ============================================================================
// KinematicContact.h - MPM 式运动学接触 (质心动量投影法)
//
// 算法思想（源自 MPM kinematic contact）：
//   对于每个 slave 粒子 i，收集所有与之接触的 master 粒子 {j}，
//   计算多体质心法向速度 v_cm，然后一次性施加碰撞修正：
//     Δv_i = (1+e) * (v_cm - v_i·n) * n
//   这避免了逐对冲量叠加导致的爆炸问题。
//
// 注意：此算法需要两段式循环（收集+投影），无法走 StandardContact 管线，
//       因此直接继承 IContactAlgorithm 并自包含空间搜索逻辑。
// ============================================================================

#include "IContactAlgorithm.h"
#include <Eigen/Dense>
#include <vector>

namespace PDCommon::Contact {

class KinematicContact : public IContactAlgorithm {
public:
  explicit KinematicContact(const std::string &name = "KinematicContact");
  ~KinematicContact() override = default;

  std::string getTypeName() const override { return "KinematicContact"; }
  void initialize(const YAML::Node &configNode) override;
  void computeContactForce(PDCommon::Core::PDContext &ctx) override;

private:
  double restitutionCoeff_ = 0.0; ///< 恢复系数 (0=完全非弹性, 1=完全弹性)
  double pinballRatio_ = 1.0;     ///< 接触探测容差放大乘子
  double frictionCoeff_ = 0.0;    ///< 库伦滑动摩擦系数

  // --- 自包含的空间哈希网格（从已废弃的 NodeNodeContact 搬入） ---
  double cellSize_ = 0.0;
  Eigen::Vector3d minBounds_;
  Eigen::Vector3d maxBounds_;
  Eigen::Vector3i gridDims_;
  std::vector<int> head_;
  std::vector<int> next_;

  void buildCellList(const double *coords, const double *disp,
                     double maxDx);
  int computeCellHash(double x, double y, double z) const;
};

} // namespace PDCommon::Contact

#endif // PDCOMMON_CONTACT_KINEMATICCONTACT_H
