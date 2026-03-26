// ============================================================================
// StaggeredIntegrator.h - 交错积分器（异阶多场耦合编排器）
//
// 职责：
//   当不同物理场需要不同阶次的时间积分方案时（如热场用一阶 ExplicitEuler，
//   力场用二阶 CentralDifference），本类作为顶层编排器，在每个大时间步内
//   依次调度各子积分器驱动其对应的内核。
//
// 使用方式：
//   YAML 中指定 TimeIntegrator: Staggered，并通过 SubSolvers 列表
//   声明每个内核与其子积分器的配对关系。
//
// YAML 示例：
//   Solver:
//     TimeIntegrator: Staggered
//     SubSolvers:
//       - Kernel: NOSB_T
//         TimeIntegrator: Explicit
//       - Kernel: NOSB_M
//         TimeIntegrator: CentralDifference
// ============================================================================

#ifndef SRC_INTEGRATION_STAGGERED_INTEGRATOR_H
#define SRC_INTEGRATION_STAGGERED_INTEGRATOR_H

#include "TimeIntegrator.h"
#include <memory>
#include <string>
#include <vector>

namespace PDCommon::Kernel { class PDKernel; }

namespace Src::Integration {

using PDCommon::Kernel::PDKernel;

/// @brief 子求解器配对单元：一个内核绑定一个专属的时间积分器
struct SubSolverUnit {
  std::string kernelName;                        ///< 内核注册名（用于匹配）
  std::unique_ptr<TimeIntegrator> integrator;    ///< 该内核专属的子积分器
};

/// @brief 交错积分器 - 异阶多场协同编排器
/// @details
///   在 run() 中，根据 SubSolverUnit 的配对关系，将 kernels 容器中的
///   每个内核路由到其专属的子积分器分别驱动。
///   弱耦合时每步交替一次；强耦合可扩展为子步迭代至收敛。
class StaggeredIntegrator : public TimeIntegrator {
public:
  StaggeredIntegrator() = default;
  ~StaggeredIntegrator() override = default;

  /// @brief 从 YAML 的 Solver 段读取 SubSolvers 配对信息
  void configure(const YAML::Node &solverNode) override;

  /// @brief 执行交错时间推进循环
  void run(PDCommon::Core::PDContext &ctx,
           std::vector<std::unique_ptr<PDKernel>> &kernels,
           std::function<void(int, double)> outputCallback) override;

  std::string getName() const override { return "StaggeredIntegrator"; }

private:
  /// @brief 子求解器配对列表（configure 阶段填充）
  std::vector<SubSolverUnit> subUnits_;
};

} // namespace Src::Integration

#endif // SRC_INTEGRATION_STAGGERED_INTEGRATOR_H
