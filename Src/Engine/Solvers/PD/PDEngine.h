// ============================================================================
// PDSolver.h - 近场动力学 (PD) 求解器
// 继承 Src::Engine::Engine，封装完整的 PD 仿真生命周期
//
// 架构：三层多态组装器
//   L1 TimeIntegrator (integrator_)  — 求解算法
//   L2 PDKernel       (kernels_)     — PD 积分框架（支持多核协同）
//   L3 Material                      — 本构模型（由 PDContext 持有）
// ============================================================================

#ifndef SRC_ENGINE_SOLVERS_PD_PDENGINE_H
#define SRC_ENGINE_SOLVERS_PD_PDENGINE_H

#include "Engine.h"
#include "PDContext.h"
#include "PDKernel.h"
#include "TimeIntegrator.h"
#include <memory>
#include <vector>

namespace Src::Engine::Solvers::PD {

/// @brief PD（近场动力学）求解引擎
/// @details 封装了 PD 仿真的完整流程：模型生成、材料分配、物理场注册、
///          边界条件加载、时间步求解与结果输出。
///          通过 REGISTER_ENGINE_TYPE 宏自动注册到 EngineRegistry。
class PDEngine : public Src::Engine::Engine {
public:
  PDEngine();
  ~PDEngine() override = default;

  // -----------------------------------------------------------------------
  // Engine 接口实现
  // -----------------------------------------------------------------------

  /// @brief 初始化 PD 仿真上下文（模型、材料、场、边界条件）
  void Initialize(const std::string &yamlPath) override;

  /// @brief 执行 PD 求解主循环
  void Solve() override;

  /// @brief 导出 PD 计算结果 (最终结果调用)
  void Output() override;

  /// @brief 导出带有时间步后缀和物理时间后缀的计算结果
  void Output(int step, double physicalTime);

  /// @brief 返回求解器类型名
  std::string getName() const override { return "PD"; }

  /// @brief 打印 PD 引擎使用的所有注册表信息
  void printRegistrySummary() const override;

private:

  // -----------------------------------------------------------------------
  // PD 仿真上下文与三层组件
  // -----------------------------------------------------------------------
  PDCommon::Core::PDContext pdContext_;              ///< PD 数据容器
  std::string yamlPath_;                                            ///< YAML 配置文件路径
  std::unique_ptr<Src::Integration::TimeIntegrator> integrator_;    ///< L1: 时间推进策略
  std::vector<std::unique_ptr<PDCommon::Kernel::PDKernel>> kernels_; ///< L2: PD 积分核心集合（多核协同）
};

} // namespace Src::Engine::Solvers::PD

#endif // SRC_ENGINE_SOLVERS_PD_PDENGINE_H
