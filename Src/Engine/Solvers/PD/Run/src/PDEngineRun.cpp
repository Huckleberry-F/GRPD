#include "PDEngineRun.h"
#include "Logger.h"

namespace Src::Engine::Solvers::PD::Run {

// ============================================================================
// 核心运行模块单入口门面实现（多核协同版本）
// 由于时间积分（如 Euler/Verlet）的 for 循环机制和收敛判断内嵌在基类中，
// 本处的门面仅承担检查装配组件完整性，随后立刻调用 TimeIntegrator::run。
// ============================================================================
void ExecuteSolve(
    PDCommon::Core::PDContext &ctx,
    const std::unique_ptr<Src::Integration::TimeIntegrator> &integrator,
    std::vector<std::unique_ptr<PDCommon::Kernel::PDKernel>> &kernels,
    const std::function<void(int, double)> &outputCallback) {
  
  if (!integrator || kernels.empty()) {
    LOG_ERROR("[PDEngineRun] Target solver components are missing.");
    return;
  }
  
  // 将多内核集合整体委托给积分器接管计算生命周期
  integrator->run(ctx, kernels, outputCallback);
}

} // namespace Src::Engine::Solvers::PD::Run
