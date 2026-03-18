#ifndef SRC_SOLVE_PD_NOSB_T_H
#define SRC_SOLVE_PD_NOSB_T_H

// ============================================================================
// NOSB_T.h - 热传导非常规态基近场动力学 (Thermal NOSB-PD)
//
// 职责：
//   实现 Thermal NOSB-PD 的非局部积分计算框架。
//   将 CSR 格式邻居表上的非局部积分与本构黑盒 (ThermalMaterial) 桥接。
//
// 架构关系：
//   NOSB_T（本类）
//     ├── 读取 PDContext 中的物理场、邻居表、粒子数据
//     ├── 执行 NOSB 三步非局部积分
//     └── 调用 ThermalMaterial::ComputeHeatFlux() 完成本构计算
//
// HPC 规约：
//   - 邻居表采用 1D CSR 扁平格式（offsets + neighborIds + bondLengths）
//   - 断键机制：neighborIds[k] == -1 表示已断键，遍历时跳过
//   - 循环粒子遍历使用 OpenMP 并行加速
//   - 使用 Eigen::Vector3d / Eigen::Matrix3d 进行向量/张量运算
//
// 本构解耦：
//   本类不包含任何材料参数或本构定律，纯粹是非局部积分框架。
//   本构计算通过 ThermalMaterial 基类的虚函数多态调用。
// ============================================================================

#include "PDContext.h"
#include <Eigen/Dense>

namespace Src::Solve {

/// @brief 热传导非常规态基近场动力学 (Thermal NOSB-PD) 计算框架
/// @details
///   无状态工具类，所有数据通过 PDContext 引用传入。
///   核心计算流程（三步法）：
///     Step 1: 为每个粒子计算形函数张量 K（非局部积分）
///     Step 2: 利用 K 从非局部温差场构造温度梯度 ∇T（经典量重构）
///     Step 3: 调用 ThermalMaterial::ComputeHeatFlux(∇T) 获取本构热流 q，
///             再将 q 通过非局部散度积分转化为粒子热量变化率
class NOSB_T {
public:
  NOSB_T() = default;
  ~NOSB_T() = default;

  // 禁用拷贝（无状态类无需拷贝）
  NOSB_T(const NOSB_T&) = delete;
  NOSB_T& operator=(const NOSB_T&) = delete;

  // -----------------------------------------------------------------------
  // 核心计算接口
  // -----------------------------------------------------------------------

  /// @brief 时间循环前预计算：为每个粒子计算形状张量的逆 K⁻¹ 并存入场
  /// @param ctx PD 仿真上下文
  /// @details
  ///   将计算得到的 3x3 矩阵按行优先压平为 9 分量，存入 FieldManager 中的 "ShapeTensorInv" 场
  void ComputeShapeTensors(PDCommon::Core::PDContext& ctx);

  /// @brief 执行 Thermal NOSB-PD 完整三步计算
  /// @param ctx PD 仿真上下文，包含粒子、邻居表、物理场、材料等全部数据
  /// @details
  ///   从 ctx 中获取以下数据：
  ///     - ParticleManager: 粒子坐标 (Coords)、体积 (Volume)
  ///     - NeighborList: CSR 偏移 (offsets)、邻居 ID (neighborIds)、键长 (bondLengths)
  ///     - FieldManager: 温度场 (Temperature)、温度变化率场 (TempRate)、热流场 (HeatFlux)
  ///     - MaterialManager: 通过粒子绑定的 ThermalMaterial 获取本构接口
  ///   计算完成后，TempRate 和 HeatFlux 场将被更新
  void ComputeThermalState(PDCommon::Core::PDContext& ctx);
};

} // namespace Src::Solve

#endif // SRC_SOLVE_PD_NOSB_T_H
