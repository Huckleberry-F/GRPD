// ============================================================================
// TimeIntegrator.h - 时间推进策略抽象基类 (Layer 1)
//
// 职责：
//   定义所有时间推进算法（显式 Euler、隐式 Newton、ADR 等）的统一接口。
//   PDSolver 通过此接口驱动时间循环，实现算法层与 PD 理论层的解耦。
//
// 架构位置：
//   PDSolver::Solve()
//     → integrator_->run()          ← L1: 本类
//         → kernel_->computeForceState()  ← L2: PDKernel
//             → mat->evaluate()     ← L3: Material
// ============================================================================

#ifndef SRC_SOLVE_PD_TIME_INTEGRATOR_H
#define SRC_SOLVE_PD_TIME_INTEGRATOR_H

#include "PDContext.h"
#include <functional>
#include <string>

namespace Src::Solve {

// 前置声明
class PDKernel;

/// @brief 求解器配置参数（从 YAML 解析后传入）
struct SolverConfig {
  double dt = 1.0;          ///< 时间步长
  double totalTime = 100.0; ///< 总求解时间
  int outputInterval = 10;  ///< 输出间隔（步数）
};

/// @brief 时间推进策略抽象基类
class TimeIntegrator {
public:
  virtual ~TimeIntegrator() = default;

  // 禁用拷贝
  TimeIntegrator(const TimeIntegrator &) = delete;
  TimeIntegrator &operator=(const TimeIntegrator &) = delete;

  // -----------------------------------------------------------------------
  // 核心接口
  // -----------------------------------------------------------------------

  /// @brief 执行完整的时间推进循环
  /// @param ctx            PD 仿真上下文
  /// @param kernel         PD 积分核心 (L2)
  /// @param config         求解器配置参数
  /// @param outputCallback 输出回调（由 PDSolver 提供）
  virtual void run(PDCommon::Core::PDContext &ctx, PDKernel &kernel,
                   const SolverConfig &config,
                   std::function<void()> outputCallback) = 0;

  /// @brief 获取算法名称（如 "ExplicitEuler", "ADR"）
  virtual std::string getName() const = 0;

protected:
  TimeIntegrator() = default; // 只允许子类构造
};

} // namespace Src::Solve

#endif // SRC_SOLVE_PD_TIME_INTEGRATOR_H
