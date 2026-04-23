#ifndef PDCOMMON_KERNEL_NOSB_M_H
#define PDCOMMON_KERNEL_NOSB_M_H

// ============================================================================
// NOSB_M.h - 力学非常规态基近场动力学 (Mechanical NOSB-PD)
//
// 职责：
//   实现 Mechanical NOSB-PD 的非局部积分计算框架。
//   将 CSR 格式邻居表上的非局部积分与本构黑盒 (MechanicalMaterial) 桥接。
// ============================================================================

#include "NOSB_Base.h"
#include "Stabilizer.h"
#include "MechanicalMaterial.h"
#include <memory>
#include <vector>

namespace PDCommon::Kernel {

class NOSB_M : public NOSB_Base {
public:
  NOSB_M() = default;
  ~NOSB_M() override = default;

  // -----------------------------------------------------------------------
  // PDKernel 接口实现
  // -----------------------------------------------------------------------

  /// @brief 时间循环前预计算：计算形状张量 K⁻¹ 并注册工作场
  void preCompute(PDCommon::Core::PDContext &ctx) override;

  /// @brief 每步核心积分：NOSB 三步非局部力学计算
  void computeForceState(PDCommon::Core::PDContext &ctx) override;

  /// @brief 时间步积分完成后的善后钩子（调用损伤等后处理计算）
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

  /// @brief 执行 Mechanical NOSB-PD 完整三步积分
  void ComputeMechanicalState(PDCommon::Core::PDContext &ctx);
};

} // namespace PDCommon::Kernel

#endif // PDCOMMON_KERNEL_NOSB_M_H
