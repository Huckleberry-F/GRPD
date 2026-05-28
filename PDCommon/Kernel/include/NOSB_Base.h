#ifndef PDCOMMON_KERNEL_NOSB_BASE_H
#define PDCOMMON_KERNEL_NOSB_BASE_H

#include "PDKernel.h"
#include <Eigen/Dense>
#include <cmath>
#include <yaml-cpp/yaml.h>

namespace PDCommon::Kernel {

// 旧的 enum 已经被重构为基于独立类的策略模式 (ThermalStabilizers.h)
// 这里只保留从 YAML 读取的字符串形式，在派生类中具体工厂实例化

/// @brief 非常规态基近场动力学 (NOSB-PD) 通用基类
/// 提供可复用的纯几何张量积分计算（例如形状张量 K^-1 的计算）。
class NOSB_Base : public PDKernel {
public:
  NOSB_Base() = default;
  ~NOSB_Base() override = default;

  /// @brief 从 YAML Solver 段读取 NOSB 家族共有的配置参数
  void configure(const YAML::Node &solverNode) override;

protected:
  std::string zeroEnergyMethodStr_{}; // 延迟初始化，由派生类在 configure 中设置
  double zeroEnergyG0_{1.0};
  double zeroEnergyPlasticFloor_{-1.0};
  double zeroEnergyPlasticRate_{-1.0};
  bool zeroEnergyDamageCoupling_{true};
  bool dynamicShapeTensor_{true};

  /// @brief 计算物质点部分体积截断修正因子
  double GetPartialVolumeFactor(double xi, double horizon, double radij) const {
    if (xi < horizon - radij)
      return 1.0;
    else if (xi < horizon + radij)
      return (horizon + radij - xi) / (2.0 * radij);
    else
      return 0.0;
  }

  /// @brief 计算形状张量逆 K⁻¹ 并存入 "ShapeTensorInv" 场（初始分配与计算）
  void ComputeShapeTensors(PDCommon::Core::PDContext &ctx);

  /// @brief 动态刷新形状张量及其逆矩阵（仅依赖当前存活的键）
  void UpdateShapeTensors(PDCommon::Core::PDContext &ctx);
};

} // namespace PDCommon::Kernel

#endif // PDCOMMON_KERNEL_NOSB_BASE_H
