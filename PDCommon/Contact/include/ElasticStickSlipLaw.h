#pragma once

#include "IFrictionLaw.h"
#include <yaml-cpp/yaml.h>
#include <unordered_map>
#include <array>

namespace PDCommon::Contact {

class ElasticStickSlipLaw : public IFrictionLaw {
public:
  ElasticStickSlipLaw() = default;
  ~ElasticStickSlipLaw() override = default;

  void initialize(const YAML::Node &configNode) override;

  std::string getTypeName() const override { return "ElasticStickSlip"; }

  void computeFriction(const ContactContext &pair, 
                       double normalForceMag, 
                       double dt,
                       double &fx, double &fy, double &fz) override;

private:
  double frictionCoeff_ = 0.0;
  double shearStiffness_ = 1.0e6; // 切向摩擦刚度

  // 用于跨步存储每对粒子的历史切向力： key (由 i, j 合并), value (ftx, fty, ftz)
  // 注意：在 NTC (点云面) 中，如果主面代表点跳跃，记录可能会丢失。
  std::unordered_map<uint64_t, std::array<double, 3>> historyTable_;

  // 清除失效的对子可以在外部管理进行，目前简单处理为无限累加存留
};

} // namespace PDCommon::Contact
