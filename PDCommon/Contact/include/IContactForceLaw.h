#ifndef PDCOMMON_CONTACT_ICONTACTFORCELAW_H
#define PDCOMMON_CONTACT_ICONTACTFORCELAW_H

// ============================================================================
// IContactForceLaw.h - 接触力学本构定律纯虚接口
//
// 职责：
//   只负责将探测器提供的几何信息（穿透量、法向、质量等）代入物理公式，
//   产生最终的接触力向量。与几何探测（如何发现碰撞对）完全解耦。
//
// 子类示例：
//   PenaltyForceLaw       - 线性弹簧罚函数 F = K * δ
//   ViscousForceLaw        - 弹簧 + 粘性阻尼 F = Kδ + cδ̇
//   NonlinearPenaltyForceLaw - 渐进非线性硬化
//   SillingForceLaw        - PD 短程排斥力
// ============================================================================

#include <string>
#include <yaml-cpp/yaml.h>

namespace PDCommon::Core { class PDContext; }

namespace PDCommon::Contact {

/// @brief 探测器输出的碰撞对通用上下文（几何 + 运动学信息）
/// @details 由 Detector 填充，传递给 ForceLaw。ForceLaw 不需要知道
///          这些数据是通过 NTN 距离法还是 NTS 梯度法得来的。
struct ContactContext {
  int i, j;                   ///< slave 和 master 粒子索引
  double dist;                ///< 当前距离
  double safeDist;            ///< 安全距离 (dx_i + dx_j) / 2
  double raw_penetration;     ///< safeDist - dist（侵入量，已经过截断保护）
  double nx, ny, nz;          ///< 法向单位矢量（j→i 方向）
  double mass_i, mass_j;      ///< slave 和 master 有效质量（含质量缩放）
  double dx_i, dx_j;          ///< 等效边长
  const double *vel;          ///< 速度场指针（可为 nullptr）
};

/// @brief 力学计算结果
struct ForceResult {
  double fx = 0.0, fy = 0.0, fz = 0.0; ///< 施加在 slave i 上的力
  bool valid = true;                     ///< 如果为 false，跳过本对
};

/// @brief 接触力学本构定律纯虚基类
class IContactForceLaw {
public:
  virtual ~IContactForceLaw() = default;

  /// @brief 获取力学定律类型名（如 "PenaltyForceLaw"）
  virtual std::string getTypeName() const = 0;

  /// @brief 从 YAML 配置节点初始化力学参数
  virtual void initialize(const YAML::Node &configNode) = 0;

  /// @brief 预处理钩子（在每步接触力计算前调用，用于自动估算刚度等）
  /// @param ctx 仿真上下文
  /// @param maxDx 最大等效边长
  virtual void onPreContact(PDCommon::Core::PDContext &ctx, double maxDx) {}

  /// @brief 设置主从面粒子 ID（供 onPreContact 中自动估算刚度使用）
  /// @details 默认空实现，需要材料信息的子类自行覆写
  virtual void setParticleIds(const std::vector<int> &masterIds,
                              const std::vector<int> &slaveIds) {}

  /// @brief 核心接口：根据碰撞对上下文计算接触力
  /// @param pair 碰撞对几何与运动学上下文（由 Detector 提供）
  /// @return ForceResult 施加在 slave 上的力向量
  virtual ForceResult computeForce(const ContactContext &pair) = 0;
};

} // namespace PDCommon::Contact

#endif // PDCOMMON_CONTACT_ICONTACTFORCELAW_H
