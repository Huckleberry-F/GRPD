#ifndef PDCOMMON_KERNEL_NOSB_BASE_H
#define PDCOMMON_KERNEL_NOSB_BASE_H

#include "PDKernel.h"
#include <Eigen/Dense>
#include <cmath>
#include <yaml-cpp/yaml.h>

namespace PDCommon::Kernel {

/// @brief 影响函数（核函数）类型
enum class InfluenceKernelType {
  Constant,        ///< 常数核：omega = 1.0
  InverseDistance, ///< 经典倒数核：omega = 1.0 / xi
  Linear,          ///< 线性核：omega = (1 - xi/delta)
  Quadratic,       ///< 二次核：omega = (1 - xi/delta)^2
  Cubic,           ///< 三次核：omega = (1 - xi/delta)^3
  Quartic,         ///< 四次核：omega = (1 - xi/delta)^4
  Gaussian         ///< 高斯核：omega = exp(-(xi/delta)^2)
};

/// @brief 零能模式修正方案
enum class ZeroEnergyMethod {
  Silling, ///< Silling 方法: G0 * omega * (deltaRes / |xi|^2) * Vj
  Wan,     ///< Wan 方法: (TODO: 待填入公式)
  Zhang    ///< Zhang 方法: (TODO: 待填入公式)
};

/// @brief 非常规态基近场动力学 (NOSB-PD) 通用基类
/// 提供可复用的纯几何张量积分计算（例如形状张量 K^-1 的计算）。
class NOSB_Base : public PDKernel {
public:
  NOSB_Base() = default;
  ~NOSB_Base() override = default;

  /// @brief 从 YAML Solver 段读取 NOSB 家族共有的配置参数
  void configure(const YAML::Node &solverNode) override;

protected:
  InfluenceKernelType kernelType_{InfluenceKernelType::InverseDistance};
  ZeroEnergyMethod zeroEnergyMethod_{ZeroEnergyMethod::Silling};
  double zeroEnergyG0_{1.0};

  /// @brief 获取指定核函数（影响函数）的权重值（内联：热路径百万级调用）
  double GetInfluenceWeight(double xi, double horizon,
                            InfluenceKernelType type) const {
    switch (type) {
    case InfluenceKernelType::InverseDistance:
      return (xi > 1e-16) ? (1.0 / xi) : 0.0;
    case InfluenceKernelType::Constant:
      return 1.0;
    case InfluenceKernelType::Linear: {
      double r = 1.0 - xi / horizon;
      return (r > 0.0) ? r : 0.0;
    }
    case InfluenceKernelType::Quadratic: {
      double r = 1.0 - xi / horizon;
      return (r > 0.0) ? r * r : 0.0;
    }
    case InfluenceKernelType::Cubic: {
      double r = 1.0 - xi / horizon;
      return (r > 0.0) ? r * r * r : 0.0;
    }
    case InfluenceKernelType::Quartic: {
      double r = 1.0 - xi / horizon;
      return (r > 0.0) ? r * r * r * r : 0.0;
    }
    case InfluenceKernelType::Gaussian:
      return std::exp(-(xi * xi) / (horizon * horizon));
    default:
      return 1.0;
    }
  }

  /// @brief 计算物质点部分体积截断修正因子
  /// 邻域边界处的粒子只有部分体积位于邻域球内，需要线性过渡修正
  /// @param xi 键长 |ξ|
  /// @param horizon 邻域半径 δ
  /// @param radij 粒子半径 (通常 = dx/2 = cbrt(V)/2)
  double GetPartialVolumeFactor(double xi, double horizon, double radij) const {
    if (xi < horizon - radij)
      return 1.0;
    else if (xi < horizon + radij)
      return (horizon + radij - xi) / (2.0 * radij);
    else
      return 0.0;
  }

  /// @brief 计算零能模式单键惩罚积分项（内联：热路径高频调用）
  /// @param omega 影响函数权重
  /// @param deltaState_res 非局部与局部状态差值
  /// @param xi 初始键长 |ξ|
  /// @param vj 邻居粒子体积
  /// @param G 惩罚系数 (Silling 方法中 G = G0 * k)
  /// @param mm 粒子 i 的加权体积 mi = Σ(ω * Vj)
  /// @param horizon 邻域半径 δ
  double ComputeZeroEnergyModePenalty(double omega, double deltaState_res,
                                      double xi, double vj, double G,
                                      double mm, double horizon) const {
    double xi2 = xi * xi;
    if (xi2 < 1e-16)
      return 0.0;

    switch (zeroEnergyMethod_) {
    case ZeroEnergyMethod::Silling: {
      // Silling: G0 * ω * (6k / πδ⁴) * (ΔTres / mi) * Vj
      double delta4 = horizon * horizon * horizon * horizon;
      double coeff = 6.0 * G / (3.141592653589793 * delta4);
      return omega * coeff * (deltaState_res / mm) * vj;
    }
    case ZeroEnergyMethod::Wan:
      // TODO: 填入 Wan 方法的公式
      return omega * G * (deltaState_res / xi2) * vj;
    case ZeroEnergyMethod::Zhang:
      // TODO: 填入 Zhang 方法的公式
      return omega * G * (deltaState_res / xi2) * vj;
    default:
      return omega * G * (deltaState_res / xi2) * vj;
    }
  }

  /// @brief 计算形状张量逆 K⁻¹ 并存入 "ShapeTensorInv" 场
  /// 这个算子纯几何依赖，热 (NOSB_T) 和 力 (NOSB_Mechanical) 均可继承复用
  void ComputeShapeTensors(PDCommon::Core::PDContext &ctx);
};

} // namespace PDCommon::Kernel

#endif // PDCOMMON_KERNEL_NOSB_BASE_H
