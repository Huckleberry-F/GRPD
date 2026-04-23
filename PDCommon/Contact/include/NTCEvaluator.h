#ifndef PDCOMMON_CONTACT_NTCEVALUATOR_H
#define PDCOMMON_CONTACT_NTCEVALUATOR_H

#include "IContactEvaluator.h"

namespace PDCommon::Contact {

/// @brief 节点对面元云(Node-To-Cloud)接触评判器
/// @details 从搜索网格中提取发生穿透的所有主面点云，通过加权平均
///          给出一个等效的宏观接触面法向、等效穿透量及等效质量。
///          适合解决表面不光滑带来的高频振荡，常配合大位移滑动使用。
class NTCEvaluator : public IContactEvaluator {
public:
  NTCEvaluator() = default;
  ~NTCEvaluator() override = default;

  std::string getTypeName() const override { return "NTC"; }

  void initialize(const YAML::Node &configNode) override;

  void onPreEvaluate(PDCommon::Core::PDContext &ctx, double maxDx) override;

  std::vector<ContactContext> evaluate(
      int slaveId, const ContactSpatialGrid &grid, PDCommon::Core::PDContext &ctx) override;

  void setMassScaleFactor(double sf) override { massScaleFactor_ = sf; }

private:
  double pinballRatio_ = 1.0;
  
  const double *coords_ = nullptr;
  const double *disp_ = nullptr;
  const double *vols_ = nullptr;
  const double *vel_ = nullptr;
  const int *partIDs_ = nullptr;
  int dim_ = 3;
  double thickness_ = 1.0;
  double massScaleFactor_ = 1.0;
};

} // namespace PDCommon::Contact

#endif // PDCOMMON_CONTACT_NTCEVALUATOR_H
