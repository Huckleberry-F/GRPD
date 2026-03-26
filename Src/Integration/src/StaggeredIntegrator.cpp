// ============================================================================
// StaggeredIntegrator.cpp - 交错积分器实现（异阶多场耦合编排器）
//
// TODO: 本文件为骨架实现，核心调度逻辑待力学模块 (NOSB_M) 完成后填充。
// ============================================================================

#include "StaggeredIntegrator.h"
#include "BCManager.h"
#include "FieldManager.h"
#include "Logger.h"
#include "PDKernel.h"
#include "ParticleManager.h"
#include "TimeIntegratorRegistry.h"
#include "TypedField.h"
#include <chrono>

namespace Src::Integration {

// ---------------------------------------------------------------------------
// configure: 从 YAML 的 Solver.SubSolvers 读取内核-积分器配对关系
// ---------------------------------------------------------------------------
void StaggeredIntegrator::configure(const YAML::Node &solverNode) {
  // 先调用基类读取公共参数（dt, totalTime, outputInterval）
  TimeIntegrator::configure(solverNode);

  // 解析 SubSolvers 配对列表
  if (!solverNode["SubSolvers"] || !solverNode["SubSolvers"].IsSequence()) {
    LOG_ERROR("[StaggeredIntegrator] Solver.SubSolvers not found or not a list! "
              "Staggered mode requires SubSolvers configuration.");
    return;
  }

  for (const auto &sub : solverNode["SubSolvers"]) {
    SubSolverUnit unit;

    // 读取内核名称（用于后续在 kernels 容器中匹配）
    if (sub["Kernel"]) {
      unit.kernelName = sub["Kernel"].as<std::string>();
    } else {
      LOG_ERROR("[StaggeredIntegrator] SubSolver entry missing 'Kernel' field!");
      continue;
    }

    // 读取子积分器类型并通过工厂创建
    if (sub["TimeIntegrator"]) {
      std::string subIntType = sub["TimeIntegrator"].as<std::string>();
      unit.integrator = TimeIntegratorRegistry::getInstance().create(subIntType);

      if (unit.integrator) {
        // 将公共参数（dt, totalTime 等）传递给子积分器
        unit.integrator->configure(solverNode);
        LOG_INFO("[StaggeredIntegrator] Paired Kernel '" + unit.kernelName +
                 "' with TimeIntegrator '" + subIntType + "'");
      } else {
        LOG_ERROR("[StaggeredIntegrator] Failed to create sub-integrator: " +
                  subIntType);
        continue;
      }
    } else {
      LOG_ERROR("[StaggeredIntegrator] SubSolver entry missing "
                "'TimeIntegrator' field!");
      continue;
    }

    subUnits_.push_back(std::move(unit));
  }

  LOG_INFO("[StaggeredIntegrator] Configured with " +
           std::to_string(subUnits_.size()) + " sub-solver units.");
}

// ---------------------------------------------------------------------------
// run: 交错调度主循环
// ---------------------------------------------------------------------------
void StaggeredIntegrator::run(
    PDCommon::Core::PDContext &ctx,
    std::vector<std::unique_ptr<PDKernel>> &kernels,
    std::function<void(int, double)> outputCallback) {

  if (subUnits_.empty()) {
    LOG_ERROR("[StaggeredIntegrator] No sub-solver units configured! "
              "Cannot proceed.");
    return;
  }

  const double dt = dt_;
  const int totalSteps = static_cast<int>(totalTime_ / dt_);
  const int outputInterval = outputInterval_;

  // =================================================================
  // 建立内核名 → kernels 容器索引的映射
  // =================================================================
  // TODO: 目前通过 Kernel 的注册名进行匹配。
  //       需要在 PDKernel 基类中增加 getName() 接口，
  //       或者在 InitSolverComponents 中记录每个 kernel 的注册名。
  //       当前骨架暂时按 subUnits_ 顺序与 kernels 顺序一一对应。

  if (subUnits_.size() != kernels.size()) {
    LOG_ERROR("[StaggeredIntegrator] SubSolver count (" +
              std::to_string(subUnits_.size()) +
              ") != Kernel count (" +
              std::to_string(kernels.size()) +
              "). Please ensure each kernel has a matching SubSolver entry.");
    return;
  }

  // =================================================================
  // 预计算阶段：遍历所有内核执行一次性初始化
  // =================================================================
  for (auto &kernel : kernels) {
    kernel->preCompute(ctx);
  }

  LOG_INFO("[StaggeredIntegrator] Starting staggered time loop: dt = " +
           std::to_string(dt) + ", totalSteps = " + std::to_string(totalSteps));

  // =================================================================
  // 主时间步循环
  // =================================================================
  auto tStart = std::chrono::high_resolution_clock::now();

  for (int step = 0; step <= totalSteps; ++step) {

    // (A) 输出
    if (step % outputInterval == 0) {
      auto tNow = std::chrono::high_resolution_clock::now();
      double elapsed = std::chrono::duration<double>(tNow - tStart).count();
      LOG_INFO("--- Staggered Step " + std::to_string(step) + " / " +
               std::to_string(totalSteps) +
               "  |  Elapsed: " + std::to_string(elapsed) + "s");
      if (outputCallback)
        outputCallback(step, step * dt);
    }
    if (step == totalSteps)
      break;

    // (B) 交错调度：依次驱动每个子系统完成一个时间步
    // TODO [核心逻辑]:
    //   当前为顺序交替（弱耦合），每个子积分器独立推进一步。
    //   强耦合扩展：可在此处包裹迭代循环，检查收敛后再进入下一个大步。
    //
    //   具体实现需要：
    //   1. 为每个子积分器单独封装"单步推进"方法（当前 run() 是完整循环）
    //   2. 或者将 run() 拆分为 initLoop() + stepOnce() + finalizeLoop()
    //   3. 在 stepOnce() 中调用对应 kernel 的 computeForceState + postCompute
    //
    //   暂时留空，待力学模块就绪后填充。

    for (size_t i = 0; i < subUnits_.size(); ++i) {
      // TODO: 调用子积分器的单步推进
      // subUnits_[i].integrator->stepOnce(ctx, *kernels[i]);
      kernels[i]->computeForceState(ctx);
      kernels[i]->postCompute(ctx);
    }
  }

  auto tEnd = std::chrono::high_resolution_clock::now();
  double totalElapsed = std::chrono::duration<double>(tEnd - tStart).count();
  LOG_INFO("[StaggeredIntegrator] Finished. Total: " +
           std::to_string(totalElapsed) + "s");
}

} // namespace Src::Integration

REGISTER_TIME_INTEGRATOR(Staggered, Src::Integration::StaggeredIntegrator)
