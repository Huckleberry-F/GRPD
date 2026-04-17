#ifndef PDCOMMON_CONTACT_NTNCONTACT_H
#define PDCOMMON_CONTACT_NTNCONTACT_H

// ============================================================================
// NTNContact.h - 节点对节点(NTN)接触算法
//
// 职责：
//   内置基于空间哈希网格的点对点碰撞检测，并在发现碰撞对时，
//   直接调用内部封装的 ForceLaw (罚函数、非线性等) 进行解耦的受力计算与施加。
// ============================================================================

#include "IContactAlgorithm.h"
#include "IContactForceLaw.h"
#include <Eigen/Dense>
#include <memory>
#include <vector>

namespace PDCommon::Contact {

class NTNContact : public IContactAlgorithm {
public:
  explicit NTNContact(const std::string &name, std::unique_ptr<IContactForceLaw> forceLaw);
  ~NTNContact() override = default;

  std::string getTypeName() const override { return "NTN"; }
  void initialize(const YAML::Node &configNode) override;
  void computeContactForce(PDCommon::Core::PDContext &ctx) override;

private:
  std::unique_ptr<IContactForceLaw> forceLaw_;

  // --- 自包含的空间哈希网格搜寻数据 ---
  double cellSize_ = 0.0;
  Eigen::Vector3d minBounds_;
  Eigen::Vector3d maxBounds_;
  Eigen::Vector3i gridDims_;
  std::vector<int> head_;
  std::vector<int> next_;
  double lastMaxDx_ = 0.0;

  void buildCellList(const double *coords, const double *disp, double maxDx);
  int computeCellHash(double x, double y, double z) const;
};

} // namespace PDCommon::Contact

#endif // PDCOMMON_CONTACT_NTNCONTACT_H
