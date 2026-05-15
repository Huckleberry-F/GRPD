#ifndef PDCOMMON_KERNEL_NOSB_AXIS_H
#define PDCOMMON_KERNEL_NOSB_AXIS_H

// ============================================================================
// NOSB_Axis.h - 轴对称非常规态基近场动力学 (Axisymmetric NOSB-PD)
//
// 职责：
//   实现 Axisymmetric NOSB-PD 的非局部积分计算框架。
//   采用 2D 降维 + 环向修正策略：粒子布置在 2D 平面上 (R, Z)，
//   并精确补全环向形变 F33 与环向附加体积力。
// ============================================================================

#include "NOSB_Base.h"
#include "Stabilizer.h"
#include "MechanicalMaterial.h"
#include <memory>
#include <vector>

namespace PDCommon::Kernel {

class NOSB_Axis : public NOSB_Base {
public:
  NOSB_Axis() = default;
  ~NOSB_Axis() override = default;

  // -----------------------------------------------------------------------
  // PDKernel 接口实现
  // -----------------------------------------------------------------------

  /// @brief 时间循环前预计算：计算形状张量 K⁻¹ 并注册工作场
  void preCompute(PDCommon::Core::PDContext &ctx) override;

  /// @brief 每步核心积分：NOSB_Axis 三步非局部力学计算（附带环向修正）
  void computeForceState(PDCommon::Core::PDContext &ctx) override;

  /// @brief 时间步积分完成后的善后钩子
  void postCompute(PDCommon::Core::PDContext &ctx) override;

  /// @brief 返回此核心需要时间积分的场对接签名
  std::vector<IntegrationTarget> getIntegrationTargets() const override;

private:
  std::unique_ptr<Stabilizer> stabilizer_;

  // =======================================================================
  // HPC 优化缓存：避免每次调用堆分配，避免内层循环重复矩阵运算
  // =======================================================================
  std::vector<PDCommon::Material::MechanicalMaterial*> matArrCache_;
  std::vector<double> rhoArrCache_;
  // 预计算的有效应力张量 S = P * K^-1，避免 O(N * NumNeighbors) 复杂度的冗余外积
  std::vector<double> pkKinvCache_;

  // -----------------------------------------------------------------------
  // 内部实现方法
  // -----------------------------------------------------------------------

  /// @brief 执行 Axisymmetric NOSB-PD 完整积分与修正
  void ComputeAxisymmetricState(PDCommon::Core::PDContext &ctx);
};

} // namespace PDCommon::Kernel

#endif // PDCOMMON_KERNEL_NOSB_AXIS_H
