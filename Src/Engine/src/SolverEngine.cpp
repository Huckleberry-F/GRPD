// ============================================================================
// SolverEngine.cpp - 仿真引擎主控类实现
// ============================================================================

#include "SolverEngine.h"

#include "Logger.h"

namespace GRPD::Engine {

SolverEngine::SolverEngine() : pdContext_("GRPD_Model") {
  LOG_INFO("==================================================");
  LOG_INFO("   GRPD Solver Engine Initialized                 ");
  LOG_INFO("==================================================");
}

void SolverEngine::Initialize(const std::string &yamlPath) {
  LOG_INFO("Entering Pre-processing Phase (Model & Material Initialization)");

  // 1. 调用底层函数生成/读取包含粒子和几何信息的纯数据上下文
  this->InitModelPD();

  // 2. 初始化材料
  this->InitMaterialPD();

  // 3. 注册物理场与材料状态变量
  this->InitFieldsPD();

  // 4. 初始化边界条件与载荷
  this->InitConditionsPD();

  LOG_INFO("PD Simulation context successfully initialized.");
}

void SolverEngine::Solve() {
  LOG_INFO("Entering Main Solver Phase");

  // 执行计算循环
  this->SolvePD();

  LOG_INFO("Main Solver Phase finished.");
}

void SolverEngine::Output() {
  LOG_INFO("Entering Post-processing Phase (Exporting Data)");

  // 将物理宇宙的数据传到底层 IO 模块写出
  this->OutputPD();
}

} // namespace GRPD::Engine
