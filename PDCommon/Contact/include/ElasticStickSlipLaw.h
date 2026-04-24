#pragma once

#include "IFrictionLaw.h"
#include <yaml-cpp/yaml.h>

// 前置声明（避免头文件循环依赖）
namespace PDCommon::Core {
class PDContext;
}

namespace PDCommon::Contact {

/// @brief 弹性粘滑摩擦定律 (Elastic Stick-Slip Friction Law)
/// @details 基于 Radial Return 弹塑性返回映射算法实现库仑摩擦。
///          切向历史力场挂载在 Slave 粒子上（通过 FieldManager 管理），
///          实现 NTS 大滑移下的连续表面过渡，不依赖特定 Master 粒子 ID。
///          内置接触活跃标记场，自动清除脱离接触后的残留历史。
class ElasticStickSlipLaw : public IFrictionLaw {
public:
  ElasticStickSlipLaw() = default;
  ~ElasticStickSlipLaw() override = default;

  void initialize(const YAML::Node &configNode) override;

  std::string getTypeName() const override { return "ElasticStickSlip"; }

  /// @brief 预处理钩子：注册/获取切向力场与接触活跃标记场
  /// @details 每步开始时，对上一步未接触的粒子清零切向力历史，
  ///          防止脱离后重新接触时产生虚假摩擦冲击。
  void onPreContact(PDCommon::Core::PDContext &ctx) override;

  void computeFriction(const ContactContext &pair, 
                       double normalForceMag, 
                       double dt,
                       double &fx, double &fy, double &fz) override;

private:
  double frictionCoeff_ = 0.0;
  double shearStiffness_ = 0.0; // 如果大于0，强制使用该固定刚度
  int stiffnessMode_ = 0; // 0: Abaqus风格(动态滑移率), 1: Ansys风格(法向刚度比例)
  double allowableSlipRatio_ = 0.005; // Abaqus 默认最大允许弹性滑移率 (0.5% dx)
  double ansysKTNRatio_ = 1.0; // Ansys 风格下的刚度比例 (KTS/KTN)

  /// @brief 切向历史力场裸指针（由 FieldManager 持有生命周期）
  /// @details 3 维度场，按 [particleID * 3 + axis] 布局，
  ///          存储每个 Slave 粒子在全局坐标系下的累积切向力。
  double* shearForceData_ = nullptr;

  /// @brief 接触活跃标记场裸指针（由 FieldManager 持有生命周期）
  /// @details 1 维度 int 场。每步开始时全部重置为 0，
  ///          computeFriction 中置为 1。下一步 onPreContact 中
  ///          对标记仍为 0 的粒子清零切向力，防止残留历史。
  int* contactActiveData_ = nullptr;

  /// @brief 粒子总数缓存（用于 onPreContact 中的遍历）
  size_t numParticles_ = 0;
};

} // namespace PDCommon::Contact
