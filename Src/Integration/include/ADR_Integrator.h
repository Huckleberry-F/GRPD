// ============================================================================
// ADR_Integrator.h - 自适应动态弛豫准静态积分器
//
// 职责：
//   实现 "Smooth Ramp + Hold & Converge" 的显式准静态时间推进架构。
//   通过质量比例阻尼和平滑升压函数，在显式框架内实现准静态加载，
//   消除瞬态冲击波，使断裂沿预定路径稳定扩展。
//
// 架构位置：
//   继承自 TimeIntegrator，通过 REGISTER_TIME_INTEGRATOR 宏注入注册表。
//   YAML 中配置 TimeIntegrator: "ADR" 即可触发。
//
// 算法核心：
//   外层循环：LoadStep (物理载荷步，每步输出一次 VTK)
//   内层循环：显式中心差分推进 + 质量比例阻尼
//     Phase 1 (Ramp): t < T_ramp 时，loadFactor 由 Smooth Step 函数驱动
//     Phase 2 (Hold): t >= T_ramp 后，loadFactor 锁定为 1.0，等动能衰减
//     收敛判定：动能 / 峰值动能 < Energy_Tol
// ============================================================================

#ifndef SRC_INTEGRATION_ADR_INTEGRATOR_H
#define SRC_INTEGRATION_ADR_INTEGRATOR_H

#include "TimeIntegrator.h"
#include <memory>
#include <vector>

namespace PDCommon::Kernel { class PDKernel; }

namespace Src::Integration {

using PDCommon::Kernel::PDKernel;

/// @brief 自适应动态弛豫准静态积分器
class ADR_Integrator : public TimeIntegrator {
public:
  ADR_Integrator() = default;
  ~ADR_Integrator() override = default;

  /// @brief 从 YAML 读取 ADR 专属控制参数
  void configure(const YAML::Node &solverNode) override;

  /// @brief 执行 LoadStep -> Smooth Ramp -> Hold & Converge 主循环
  void run(PDCommon::Core::PDContext &ctx,
           std::vector<std::unique_ptr<PDKernel>> &kernels,
           std::function<void(int, double)> outputCallback) override;

  std::string getName() const override { return "ADR"; }

private:
  // -----------------------------------------------------------------------
  // ADR 专属参数（从 YAML Solver 段读取）
  // -----------------------------------------------------------------------
  int numLoadSteps_ = 10;       ///< 物理载荷大步数量
  int numSubsteps_  = 1;        ///< 每个载荷步细分为几个增量子步
  int kbc_          = 0;        ///< 加载控制：0=Ramp(坡道加载), 1=Step(阶跃突加)
  int maxPseudoSteps_ = 5000;   ///< 每个 Substep 内允许的最大迭代数
  double dispTol_   = 1.0e-6;   ///< 位移收敛阈值 TOL2
  double forceTol_  = 1.0e-4;   ///< 力平衡收敛阈值 TOL3
};

} // namespace Src::Integration

#endif // SRC_INTEGRATION_ADR_INTEGRATOR_H
