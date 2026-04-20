#ifndef PDCOMMON_CONTACT_IFRICTIONLAW_H
#define PDCOMMON_CONTACT_IFRICTIONLAW_H

#include "IContactForceLaw.h"
#include <yaml-cpp/yaml.h>
#include <string>

namespace PDCommon::Contact {

/// @brief 切向阻滞本构定律接口 (Axis D)
/// @details 负责求解接触面的滑动阻力与静止咬合（摩擦力），
///          其计算的物理极限受限于轴 C 提供的法向排斥力 normalForceMag。
class IFrictionLaw {
public:
  virtual ~IFrictionLaw() = default;

  /// @brief 获取摩擦本构类型名（如 "ExplicitImpulseLaw"）
  virtual std::string getTypeName() const = 0;

  /// @brief 从 YAML 初始化参数（如 frictionCoeff 等）
  virtual void initialize(const YAML::Node &configNode) = 0;

  /// @brief 计算切向摩擦力并叠加到(fx, fy, fz)上
  /// @param pair 从轴 B 提取出的通用几何与运动学上下文
  /// @param normalForceMag 从轴 C 算出的本分步法向排斥力大小
  /// @param dt 积分步长，用于隐式和冲量截断等时间相关计算
  /// @param [in,out] fx 累计接触力X分量（将在此叠加摩擦力）
  /// @param [in,out] fy 累计接触力Y分量
  /// @param [in,out] fz 累计接触力Z分量
  virtual void computeFriction(const ContactContext &pair, 
                               double normalForceMag, 
                               double dt,
                               double &fx, double &fy, double &fz) = 0;
};

} // namespace PDCommon::Contact

#endif // PDCOMMON_CONTACT_IFRICTIONLAW_H
