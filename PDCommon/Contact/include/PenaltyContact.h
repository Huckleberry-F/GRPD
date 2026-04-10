#ifndef PDCOMMON_CONTACT_PENALTYCONTACT_H
#define PDCOMMON_CONTACT_PENALTYCONTACT_H

// ============================================================================
// PenaltyContact.h - 惩罚函数自接触算法与动态网格搜索
// ============================================================================

#include "IContactAlgorithm.h"
#include <Eigen/Dense>
#include <vector>

namespace PDCommon::Contact {

class PenaltyContact : public IContactAlgorithm {
public:
  explicit PenaltyContact(const std::string &name = "PenaltyContact");
  ~PenaltyContact() override = default;

  std::string getTypeName() const override { return "PenaltyContact"; }

  /// @brief 从 YAML 配置解析常数（由于是全局自接触，不需要解析 master/slave）
  void initialize(const YAML::Node &configNode) override;

  /// @brief 计算每步的排斥力并注入全局加速度场
  void computeContactForce(PDCommon::Core::PDContext &ctx) override;

private:
  // YAML 可配参数
  double penaltyFactor_ = 1.0;        // 自动惩罚刚度系数 (默认 1.0)
  double penaltyStiffness_ = -1.0;    // 法向惩罚弹簧刚度 (<0 时由代码基于体积模量自动计算)
  double pinballRatio_ = 0.5;         // 过盈探测带比例，防止极大初始穿透炸机

  // Spatial Hash Grid 状态 (DynamicCellList)
  double cellSize_ = 0.0;             // 网格大小 (d_safe 的最大值包络)
  Eigen::Vector3d minBounds_;
  Eigen::Vector3d maxBounds_;
  Eigen::Vector3i gridDims_;
  
  // CSR 链表：用于 O(N) 重构和 O(1) 搜索
  std::vector<int> head_;             // 长度为网格总数，存储每个格子内的头结点粒子ID
  std::vector<int> next_;             // 长度为全场粒子数，存储链表下一个粒子的ID

  // 辅助函数：根据体积计算等效边长 dx
  void buildCellList(const double* coords, const double* disp, const int* isSurface, const int* activeStatus, double maxDx);
  inline int computeCellHash(double x, double y, double z) const;
};

} // namespace PDCommon::Contact

#endif // PDCOMMON_CONTACT_PENALTYCONTACT_H
