// ============================================================================
// Solve.cpp - PDSolver::Solve() 通用实现
//
// 通过三层多态架构，Solve 简化为组装调用：
//   integrator_->run(ctx, *kernel_, config, outputCallback)
// ============================================================================

#include "PDSolver.h"
#include "Logger.h"

namespace Src::Solve {

void PDSolver::Solve() {
  LOG_INFO("[PDSolver] Starting PD Solver...");

  if (!integrator_ || !kernel_) {
    LOG_ERROR("[PDSolver] Solver components not initialized! "
              "Call Initialize() before Solve().");
    return;
  }

  // 输出回调：由 PDSolver 提供给 TimeIntegrator 使用
  auto outputCallback = [this]() { this->Output(); };

  // 委托给 L1 (TimeIntegrator) 驱动整个时间循环
  integrator_->run(pdContext_, *kernel_, solverConfig_, outputCallback);

  LOG_INFO("[PDSolver] PD Solver Finished.");
}

} // namespace Src::Solve
