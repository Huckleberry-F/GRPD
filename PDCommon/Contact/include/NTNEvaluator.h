#ifndef PDCOMMON_CONTACT_NTNEVALUATOR_H
#define PDCOMMON_CONTACT_NTNEVALUATOR_H

#include "IContactEvaluator.h"

namespace PDCommon::Contact {

/// @brief 传统节点对节点(Node-To-Node)接触评判器
/// @details 从搜索网格中提取出发生几何穿透的点对，每个主面点对应
///          生成一个独立的 ContactContext 供受力计算使用。
class NTNEvaluator : public IContactEvaluator {
public:
  NTNEvaluator() = default;
  ~NTNEvaluator() override = default;

  std::string getTypeName() const override { return "NTN"; }

  void initialize(const YAML::Node &configNode) override;

  void onPreEvaluate(PDCommon::Core::PDContext &ctx, double maxDx) override;

  std::vector<ContactContext> evaluate(
      int slaveId, const ContactSpatialGrid &grid, PDCommon::Core::PDContext &ctx) override;

  void setMassScaleFactor(double sf) override { massScaleFactor_ = sf; }

private:
  double pinballRatio_ = 1.0;
  
  // 缓存场指针避免循环内重复获取
  const double *coords_ = nullptr;
  const double *disp_ = nullptr;
  const double *vols_ = nullptr;
  const double *vel_ = nullptr;
  const int *partIDs_ = nullptr;
  int dim_ = 3;
  double thickness_ = 1.0;
  double massScaleFactor_ = 1.0; // 可能需要通过 ctx 或 global 获取，目前可以暂存为1
};

} // namespace PDCommon::Contact

#endif // PDCOMMON_CONTACT_NTNEVALUATOR_H
