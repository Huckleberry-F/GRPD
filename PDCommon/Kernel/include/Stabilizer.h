#ifndef PDCOMMON_KERNEL_STABILIZER_H
#define PDCOMMON_KERNEL_STABILIZER_H

#include "PDContext.h"
#include <string>

namespace PDCommon::Kernel {

// =========================================================================
// 零能模式稳定化策略基类 (Zero-Energy Mode Stabilizer Interface)
// 纯虚泛型接口：不再强制绑定具体物理场的参数签名 (如 kArr 等)。
// =========================================================================

/// @brief 近场动力学非局部方程零能模式稳定化器的基类
/// 热传导与力学计算共用相同的生命周期抽象。具体参数通过 PDContext 提取。
class Stabilizer {
public:
  virtual ~Stabilizer() = default;

  /// @brief 每时间步调用：执行独立的遍历积分，将惩罚力/热流加回 Rate 或 Force
  /// 场 子类在内部通过 ctx.getParticleManager() 和 dynamic_cast
  /// 本构类自行获取所需属性。
  virtual void applyPenalty(PDCommon::Core::PDContext &ctx) = 0;

  /// @brief 模型时间步循环前调用：由核心求解引擎调用，进行零内存分配改造。子类在此统一提取并缓存后续海量计算所需的不变量矩阵与材料属性数组。
  virtual void preCompute(PDCommon::Core::PDContext &ctx) {}

  /// @brief 设置稳定器缩放系数 G0（通常由 YAML 指定参数控制总惩罚权重）
  void setG0(double g0) { g0_ = g0; }

  /// @brief 设置质量缩放因子（与主 Kernel 保持一致）
  void setMassScaleFactor(double factor) { massScaleFactor_ = factor; }

  /// @brief 设置塑性软化系数的底限（Floor），-1.0表示回落至稳定器默认值
  void setPlasticSofteningFloor(double floor) { plasticSofteningFloor_ = floor; }

  /// @brief 设置塑性软化的指数衰减率（Rate），-1.0表示回落至默认值100.0
  void setPlasticSofteningRate(double rate) { plasticSofteningRate_ = rate; }

  /// @brief 设置是否开启零能模式与粒子损伤的耦合软化
  void setDamageCoupling(bool enabled) { damageCoupling_ = enabled; }

  void setAxisymmetricIntegration(bool enabled) {
    axisymmetricIntegration_ = enabled;
  }

  /// @brief 获取稳定器名称
  const std::string &getName() const { return name_; }
  void setName(const std::string &name) { name_ = name; }

protected:
  std::string name_;              ///< 稳定器注册名
  double g0_{1.0};                ///< 零能修正缩放系数 G0
  double massScaleFactor_{1.0};   ///< 质量缩放因子（与主 Kernel 同步）
  double plasticSofteningFloor_{-1.0}; ///< 塑性软化缩放底限，-1.0 表示未配置
  double plasticSofteningRate_{-1.0};  ///< 塑性软化衰减速率，-1.0 表示未配置
  bool damageCoupling_{true};          ///< 是否开启零能模式与粒子损伤的耦合软化，默认开启
  bool axisymmetricIntegration_{false};
};

} // namespace PDCommon::Kernel

#endif // PDCOMMON_KERNEL_STABILIZER_H
