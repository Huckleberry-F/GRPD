// ============================================================================
// PDSolver.cpp - PD 求解器生命周期管理 + 静态注册
// ============================================================================

#include "PDSolver.h"
#include "EngineRegistry.h"
#include "Logger.h"

namespace Src::Solve {

// ---------------------------------------------------------------------------
// 构造函数
// ---------------------------------------------------------------------------
PDSolver::PDSolver() {
  LOG_INFO("[PDSolver] PD Solver instance created.");
}

// ---------------------------------------------------------------------------
// Initialize: 依次调用四个分步初始化函数
// ---------------------------------------------------------------------------
void PDSolver::Initialize(const std::string &yamlPath) {
  LOG_INFO("[PDSolver] Initializing PD simulation context...");
  yamlPath_ = yamlPath;

  // 1. 模型几何与粒子生成
  InitModel();

  // 2. 材料分配与初始化
  InitMaterial();

  // 3. 物理场与状态变量注册
  InitFields();

  // 4. 边界条件与载荷加载
  InitConditions();

  // 5. 邻域搜索与 NeighborCount 场构建
  InitNeighbors();

  // 6. 从 YAML 创建求解算法 (L1) 与 PD 积分核心 (L2)
  InitSolverComponents();

  LOG_INFO("[PDSolver] PD simulation context initialized successfully.");
}

} // namespace Src::Solve

// ============================================================================
// 静态注册：将 PDSolver 注册到全局 EngineRegistry
// ============================================================================
REGISTER_ENGINE_TYPE(PD, []() {
  return std::make_unique<Src::Solve::PDSolver>();
});
