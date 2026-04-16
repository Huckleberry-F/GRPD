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
// ============================================================================

#include "NodeNodeContact.h"

namespace PDCommon::Contact {

class KinematicContact : public NodeNodeContact {
public:
  explicit KinematicContact(const std::string &name = "KinematicContact");
  ~KinematicContact() override = default;

  std::string getTypeName() const override { return "KinematicContact"; }
  void initialize(const YAML::Node &configNode) override;

  /// @brief 重写基类模板方法，实现 MPM 两阶段质心投影
  void computeContactForce(PDCommon::Core::PDContext &ctx) override;

protected:
  /// @brief KinematicContact 不使用 per-pair 接口，此处为空实现
  ContactPairResult computePairForce(const ContactPairContext &pair) override {
    return ContactPairResult{};
  }

private:
  double restitutionCoeff_ = 0.0; ///< 恢复系数 (0=完全非弹性, 1=完全弹性)
  double pinballRatio_ = 1.0;     ///< 接触包盖探测容差半径放大乘子
  double frictionCoeff_ = 0.0;    ///< 库伦滑动摩擦系数
};

} // namespace PDCommon::Contact

#endif // PDCOMMON_CONTACT_KINEMATICCONTACT_H
