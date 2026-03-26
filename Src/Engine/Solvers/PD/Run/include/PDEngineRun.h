#ifndef SRC_ENGINE_SOLVERS_PD_RUN_PDENGINERUN_H
#define SRC_ENGINE_SOLVERS_PD_RUN_PDENGINERUN_H

#include "PDContext.h"
#include "PDKernel.h"
#include "TimeIntegrator.h"
#include <memory>
#include <functional>
#include <vector>

namespace Src::Engine::Solvers::PD::Run {

/// @brief 接管 PD 引擎的主求解循环运行态逻辑
/// @details 以单入口下放主循环控制权至被构造出的时间积分器中。
/// 支持多内核协同推进，热力多尺度交替逻辑均可在此层进行二次分拆组合。
/// @param ctx PD 全局上下文
/// @param integrator 由工厂通过反射生成的多态时间递推积分器
/// @param kernels 由工厂通过反射生成的空间计算核集合（支持多场协同）
/// @param outputCallback 用于每一时间步检查写出条件的回调函数（接入后处理接口）
void ExecuteSolve(
    PDCommon::Core::PDContext &ctx,
    const std::unique_ptr<Src::Integration::TimeIntegrator> &integrator,
    std::vector<std::unique_ptr<PDCommon::Kernel::PDKernel>> &kernels,
    const std::function<void(int, double)> &outputCallback);

} // namespace Src::Engine::Solvers::PD::Run

#endif // SRC_ENGINE_SOLVERS_PD_RUN_PDENGINERUN_H
