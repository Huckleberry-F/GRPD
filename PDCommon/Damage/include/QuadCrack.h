#ifndef PDCOMMON_DAMAGE_QUADCRACK_H
#define PDCOMMON_DAMAGE_QUADCRACK_H

#include "PreCrackModel.h"
#include <Eigen/Dense>

namespace PDCommon::Damage {

/// @brief 四边形面预置裂纹 (QuadCrack)
/// 通过射线(键)与平面多边形求交判定，降维打击地覆盖 2D/3D 任意空间斜裂纹
class QuadCrack : public PreCrackModel {
public:
  QuadCrack() = default;
  ~QuadCrack() override = default;

  void configure(const YAML::Node &node) override;
  void apply(PDCommon::Core::PDContext &ctx) override;

private:
  Eigen::Vector3d v1_, v2_, v3_, v4_; // 四边形的四个顶点坐标
  Eigen::Vector3d normal_;           // 平面单位法向量

  /// @brief 几何核心：计算线段 (A, B) 是否贯穿限定的面 (V1..V4)
  bool doesIntersect(const Eigen::Vector3d &pA, const Eigen::Vector3d &pB) const;
};

} // namespace PDCommon::Damage

#endif // PDCOMMON_DAMAGE_QUADCRACK_H
