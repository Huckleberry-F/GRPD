#include "PDEnginePre.h"
#include "TimeIntegratorRegistry.h"
#include "KernelRegistry.h"
#include "Logger.h"

namespace Src::Engine::Solvers::PD::Init {

// ============================================================================
// 步骤 6：装配核心运动方程求积分器与本构内核引擎（多核版本）
// 基于 YAML 解析所选时域推演器 (TimeIntegrator，例如显式 Euler / Verlet) 
// 和空间求解核 (PDKernel)。支持单个 Kernel 或 Kernels 列表解析。
// ============================================================================
void InitSolverComponents(
    const YAML::Node &config,
    std::unique_ptr<Src::Integration::TimeIntegrator> &integrator,
    std::vector<std::unique_ptr<PDCommon::Kernel::PDKernel>> &kernels) {
  LOG_INFO("[InitSolverComponents] ==================================================");
  LOG_INFO("[InitSolverComponents] Entering Solver Components Assembly Phase...");
  LOG_INFO("[InitSolverComponents] ==================================================");

  // -----------------------------------------------------------------
  // 1. 时间积分器装配（逻辑不变）
  // -----------------------------------------------------------------
  std::string solverType;
  if (config["Solver"] && config["Solver"]["TimeIntegrator"]) {
    solverType = config["Solver"]["TimeIntegrator"].as<std::string>();
  } else {
    LOG_ERROR("Solver.TimeIntegrator not found in YAML! Please specify it explicitly.");
    exit(EXIT_FAILURE);
  }

  LOG_INFO("[InitSolverComponents] Building Time Integrator: " + solverType);
  integrator = Src::Integration::TimeIntegratorRegistry::getInstance()
                   .create(solverType);

  if (integrator) {
    integrator->configure(config["Solver"]);
    LOG_INFO("[InitSolverComponents] Time Integrator configured.");
  } else {
    LOG_ERROR("Failed to create Time Integrator: " + solverType);
    exit(EXIT_FAILURE);
  }

  // -----------------------------------------------------------------
  // 2. 空间内核装配（升级为多核支持）
  //    优先读取 Solver.Kernels 数组；若不存在则降级读取 Solver.Kernel 单值
  // -----------------------------------------------------------------
  std::vector<std::string> kernelTypes;

  if (config["Solver"] && config["Solver"]["Kernels"] && 
      config["Solver"]["Kernels"].IsSequence()) {
    // 多核列表模式：Kernels: [NOSB_T, NOSB_M]
    for (const auto &node : config["Solver"]["Kernels"]) {
      kernelTypes.push_back(node.as<std::string>());
    }
    LOG_INFO("[InitSolverComponents] Detected multi-kernel configuration with " + 
             std::to_string(kernelTypes.size()) + " kernels.");
  } else if (config["Solver"] && config["Solver"]["Kernel"]) {
    // 单核兼容模式：Kernel: NOSB_T
    kernelTypes.push_back(config["Solver"]["Kernel"].as<std::string>());
  } else {
    LOG_ERROR("Solver.Kernel (or Solver.Kernels) not found in YAML! "
              "Please specify it explicitly.");
    exit(EXIT_FAILURE);
  }

  // 逐一实例化并压入容器
  for (const auto &kernelType : kernelTypes) {
    LOG_INFO("[InitSolverComponents] Building Space Kernel: " + kernelType);
    auto kernel = PDCommon::Kernel::KernelRegistry::getInstance().create(kernelType);

    if (kernel) {
      kernel->configure(config["Solver"]);
      LOG_INFO("[InitSolverComponents] Space Kernel '" + kernelType + "' configured.");
      kernels.push_back(std::move(kernel));
    } else {
      LOG_ERROR("Failed to create Kernel: " + kernelType);
      exit(EXIT_FAILURE);
    }
  }

  LOG_INFO("[InitSolverComponents] Total kernels assembled: " + std::to_string(kernels.size()));

  // -----------------------------------------------------------------
  // TODO [热力耦合扩展]: 支持 StaggeredIntegrator（异阶交错积分器）
  // -----------------------------------------------------------------
  // 当 TimeIntegrator 为 "Staggered" 时，需要额外解析 Solver.SubSolvers 节点，
  // 将每个 Kernel 与其专属的子积分器进行配对。YAML 格式示例：
  //
  //   Solver:
  //     TimeIntegrator: Staggered
  //     SubSolvers:
  //       - Kernel: NOSB_T
  //         TimeIntegrator: Explicit        # 热场用一阶显式欧拉
  //       - Kernel: NOSB_M
  //         TimeIntegrator: CentralDifference  # 力场用二阶中心差分
  //
  // 实现要点：
  //   1. 在此处检测 solverType == "Staggered"
  //   2. 遍历 config["Solver"]["SubSolvers"] 序列
  //   3. 对每个子项分别创建 Kernel（已上方压入 kernels_）和子 TimeIntegrator
  //   4. 将配对信息传递给 StaggeredIntegrator（可通过 configure 或专用 setter）
  //   5. StaggeredIntegrator::run() 内部按配对关系分别驱动各子系统
  //   6. 交替/迭代策略（弱耦合单次交替 vs 强耦合迭代收敛）由 Staggered 内部控制
  // -----------------------------------------------------------------
}

} // namespace Src::Engine::Solvers::PD::Init
