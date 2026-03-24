#ifndef PDCOMMON_KERNEL_NOSB_BASE_H
#define PDCOMMON_KERNEL_NOSB_BASE_H

#include "PDKernel.h"
#include <Eigen/Dense>
#include <cmath>
#include <yaml-cpp/yaml.h>

namespace PDCommon::Kernel {

/// @brief 零能模式修正方案
enum class ZeroEnergyMethod {
  Silling, ///< Silling 方法: G0 * ω * (6k / πδ⁴) * (ΔTres / vv) * Vj
  Wan,     ///< Wan 方法: G0 * k * trace(K⁻¹) * ω * ΔTres * Vj
  Zhang    ///< Zhang 方法: 基于 KT = K⁻¹·D·K⁻¹ 二次型的各向异性惩罚
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
  ZeroEnergyMethod zeroEnergyMethod_{ZeroEnergyMethod::Silling};
  double zeroEnergyG0_{1.0};

  /// @brief 计算物质点部分体积截断修正因子
  double GetPartialVolumeFactor(double xi, double horizon, double radij) const {
    if (xi < horizon - radij)
      return 1.0;
    else if (xi < horizon + radij)
      return (horizon + radij - xi) / (2.0 * radij);
    else
      return 0.0;
  }

  /// @brief 计算零能模式单键惩罚积分项（Silling / Wan 方法专用）
  /// Zhang 方法因依赖键向量二次型，在 NOSB_T.cpp 内循环中直接展开
  /// @param omega 影响函数权重
  /// @param deltaState_res 非局部与局部状态差值
  /// @param xi 初始键长 |ξ|
  /// @param vj 邻居粒子体积
  /// @param G 惩罚系数 (Silling: G0 * k)
  /// @param vv 粒子的邻域体积修正比 v_horizon = Σfac / Σω_raw
  /// @param horizon 邻域半径 δ
  /// @param G_Wan Wan 方法惩罚系数 (G0 * k * trace(K⁻¹))
  double ComputeZeroEnergyModePenalty(double omega, double deltaState_res,
                                      double xi, double vj, double G, double vv,
                                      double horizon, double G_Wan) const {
    double xi2 = xi * xi;
    if (xi2 < 1e-16)
      return 0.0;

    switch (zeroEnergyMethod_) {
    case ZeroEnergyMethod::Silling: {
      // Silling: G0 * ω * (6k / πδ⁴) * (ΔTres / vv) * Vj
      double delta4 = horizon * horizon * horizon * horizon;
      double coeff = 6.0 * G / (3.141592653589793 * delta4);
      return omega * coeff * (deltaState_res / vv) * vj;
    }
    case ZeroEnergyMethod::Wan:
      return omega * G_Wan * deltaState_res * vj;
    case ZeroEnergyMethod::Zhang:
      // Zhang 方法在 NOSB_T.cpp 步骤 3 的内循环中直接展开（依赖键向量二次型）
      // 此处 fallback：使用类似 Silling 的标量形式
      return omega * G * deltaState_res * vj;
    default:
      return omega * G * deltaState_res * vj;
    }
  }

  /// @brief 计算形状张量逆 K⁻¹ 并存入 "ShapeTensorInv" 场
  /// 这个算子纯几何依赖，热 (NOSB_T) 和 力 (NOSB_Mechanical) 均可继承复用
  void ComputeShapeTensors(PDCommon::Core::PDContext &ctx);
};

} // namespace PDCommon::Kernel

#endif // PDCOMMON_KERNEL_NOSB_BASE_H
