// ============================================================================
// CentralDifference.h - 中心差分法时间积分器 (Leapfrog / Velocity Verlet)
// ============================================================================

#ifndef SRC_SOLVE_PD_CENTRAL_DIFFERENCE_H
#define SRC_SOLVE_PD_CENTRAL_DIFFERENCE_H

#include "TimeIntegrator.h"
#include <memory>
#include <vector>

namespace PDCommon::Kernel { class PDKernel; }

namespace Src::Integration {

using PDCommon::Kernel::PDKernel;

/// @brief 中心差分时间积分器 (Velocity Verlet 格式)
/// @details
///   泛型二阶常微分方程积分器，适用于任意存在 "位移/速度" 类拓扑依赖的物理场：
///   v_{n+1/2} = v_n + 0.5 * a_n * dt
///   u_{n+1}   = u_n + v_{n+1/2} * dt
///   (计算 a_{n+1})
///   v_{n+1}   = v_{n+1/2} + 0.5 * a_{n+1} * dt
///
///   此算法严格依靠 Kernel 返回的 targets 进行自动拓扑配对（例如找出 target_a 的 Primary
///   碰巧是 target_b 的 Rate）。未找到耦合并组建二阶系统的环境时，将直接报错。
class CentralDifference : public TimeIntegrator {
public:
  CentralDifference() = default;
  ~CentralDifference() override = default;

  /// @brief 执行中心差分时间推进循环（多核版本）
  void run(PDCommon::Core::PDContext &ctx,
           std::vector<std::unique_ptr<PDKernel>> &kernels,
           std::function<void(int, double)> outputCallback) override;

  std::string getName() const override { return "CentralDifference"; }
};

} // namespace Src::Integration

#endif // SRC_SOLVE_PD_CENTRAL_DIFFERENCE_H
