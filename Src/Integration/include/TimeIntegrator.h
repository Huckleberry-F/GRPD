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

namespace PDCommon::Kernel { class PDKernel; }

namespace Src::Integration {

// 前置声明
using PDCommon::Kernel::PDKernel;

#include <yaml-cpp/yaml.h>

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

  /// @brief 从 YAML 读取控制参数（子类可 override 追加私有参数）
  virtual void configure(const YAML::Node &solverNode) {
    if (solverNode["TimeStep_dt"])
      dt_ = solverNode["TimeStep_dt"].as<double>();
    if (solverNode["TotalTime"])
      totalTime_ = solverNode["TotalTime"].as<double>();
    if (solverNode["OutputInterval"])
      outputInterval_ = solverNode["OutputInterval"].as<int>();
  }

  /// @brief 执行完整的时间推进循环
  /// @param ctx            PD 仿真上下文
  /// @param kernel         PD 积分核心 (L2)
  /// @param outputCallback 输出回调（由 PDSolver 提供）
  virtual void run(PDCommon::Core::PDContext &ctx, PDKernel &kernel,
                   std::function<void(int, double)> outputCallback) = 0;

  /// @brief 获取算法名称（如 "ExplicitEuler", "ADR"）
  virtual std::string getName() const = 0;

protected:
  double dt_ = 1.0;
  double totalTime_ = 100.0;
  int outputInterval_ = 10;

  TimeIntegrator() = default; // 只允许子类构造
};

} // namespace Src::Integration

#endif // SRC_SOLVE_PD_TIME_INTEGRATOR_H
