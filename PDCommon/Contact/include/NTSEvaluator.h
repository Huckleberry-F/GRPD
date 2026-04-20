#pragma once

#include "IContactEvaluator.h"
#include <vector>
#include <yaml-cpp/yaml.h>

namespace PDCommon::Contact {

class NTSEvaluator : public IContactEvaluator {
public:
  NTSEvaluator() = default;
  ~NTSEvaluator() override = default;

  void initialize(const YAML::Node &configNode) override;

  std::string getTypeName() const override { return "NTS"; }

  double getSearchRadiusRatio() const override { return smoothRatio_; }

  void onPreEvaluate(PDCommon::Core::PDContext &ctx, double maxDx) override;

  std::vector<ContactContext> evaluate(int slaveId,
                                       const ContactSpatialGrid &grid,
                                       PDCommon::Core::PDContext &ctx) override;

  void setMassScaleFactor(double sf) override { massScaleFactor_ = sf; }

private:
  const double *coords_ = nullptr;
  const double *disp_ = nullptr;
  const double *vols_ = nullptr;
  double *vel_ = nullptr;
  double *mass_ = nullptr;
  const int *isSurface_ = nullptr;

  double pinballRatio_ = 1.0;
  double smoothRatio_ = 1.5; // 核函数提取面法相的额外扩大系数，确保点足够多
  double massScaleFactor_ = 1.0;
};

} // namespace PDCommon::Contact
