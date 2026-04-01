// ============================================================================
// ExplicitEuler.h - 显式欧拉时间积分 (Layer 1 具体实现)
// ============================================================================

#ifndef SRC_SOLVE_PD_EXPLICIT_EULER_H
#define SRC_SOLVE_PD_EXPLICIT_EULER_H

#include "TimeIntegrator.h"
#include <memory>
#include <vector>

namespace PDCommon::Kernel { class PDKernel; }

namespace Src::Integration {

using PDCommon::Kernel::PDKernel;

/// @brief 显式欧拉时间积分器
/// @details
///   T_{n+1} = T_n + Rate_n * dt
///   适用于热传导等显式时间推进问题。支持多核协同推进。
class ExplicitEuler : public TimeIntegrator {
public:
  ExplicitEuler() = default;
  ~ExplicitEuler() override = default;

  /// @brief 从 YAML 读取控制参数
  void configure(const YAML::Node &solverNode) override;

  /// @brief 执行显式欧拉时间推进循环（多核版本）
  void run(PDCommon::Core::PDContext &ctx,
           std::vector<std::unique_ptr<PDKernel>> &kernels,
           std::function<void(int, double)> outputCallback) override;

  std::string getName() const override { return "ExplicitEuler"; }
};

} // namespace Src::Integration

#endif // SRC_SOLVE_PD_EXPLICIT_EULER_H
