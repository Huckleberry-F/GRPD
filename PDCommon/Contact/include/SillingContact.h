#ifndef PDCOMMON_CONTACT_SILLINGCONTACT_H
#define PDCOMMON_CONTACT_SILLINGCONTACT_H

// ============================================================================
// SillingContact.h - Silling 短程排斥力接触算法 (Short-Range Force)
// 参考：Silling & Askari (2005) "A meshfree method based on the peridynamic
//       model of solid mechanics"
// 特点：刚度直接源自 PD 微模量 (micromodulus)，天然与近场动力学框架一致。
//       c_s = 18K / (π δ³ Δx)   (3D)
//       c_s = 9E  / (π h δ² Δx) (2D)
// ============================================================================

#include "NodeNodeContact.h"

namespace PDCommon::Contact {

class SillingContact : public NodeNodeContact {
public:
  explicit SillingContact(const std::string &name = "SillingContact");
  ~SillingContact() override = default;

  std::string getTypeName() const override { return "SillingContact"; }
  void initialize(const YAML::Node &configNode) override;

protected:
  void onPreContact(PDCommon::Core::PDContext &ctx, double maxDx) override;
  ContactPairResult computePairForce(const ContactPairContext &pair) override;

private:
  double shortRangeStiffness_ = -1.0; ///< 短程力刚度 (<0 时自动从 PD 微模量推导)
  double stiffnessFactor_ = 1.0;      ///< 刚度缩放系数（调参用）
  int dim_ = 3;                       ///< 空间维度
  double effectiveThickness_ = 1.0;   ///< 有效厚度 (2D平面投影时介入)
};

} // namespace PDCommon::Contact

#endif // PDCOMMON_CONTACT_SILLINGCONTACT_H
