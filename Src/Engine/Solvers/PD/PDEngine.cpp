// ============================================================================
// PDEngine.cpp - PD 求解引擎 (整合自 Initialize, Solve, Output)
// ============================================================================

#include "PDEngine.h"
#include "PDEnginePre.h"
#include "PDEngineRun.h"
#include "PDEnginePost.h"
#include "BCRegistry.h"
#include "EngineRegistry.h"
#include "FieldManager.h"
#include "FieldRegistry.h"
#include "IOManager.h"
#include "KernelRegistry.h"
#include "Logger.h"
#include "MaterialRegistry.h"
#include "TimeIntegratorRegistry.h"
#include "PhysicsFieldRegistry.h"
#include <string>
#include <vector>
#include <yaml-cpp/yaml.h>

namespace Src::Engine::Solvers::PD {

// ---------------------------------------------------------------------------
// 辅助：将注册表类型列表拼接为逗号分隔的字符串
// ---------------------------------------------------------------------------
static std::string joinTypes(const std::vector<std::string> &types) {
  std::string result;
  for (const auto &t : types) {
    result += t + ", ";
  }
  if (!result.empty()) {
    result.erase(result.size() - 2);
  }
  return result;
}

PDEngine::PDEngine() { 
  LOG_INFO("[PDEngine] PD Engine instance created."); 
}

// ---------------------------------------------------------------------------
// printRegistrySummary: 打印 PD 引擎使用的所有注册表信息
// ---------------------------------------------------------------------------
void PDEngine::printRegistrySummary() const {
  LOG_INFO("========== Registered Types Summary ===========");
  LOG_INFO("  TimeIntegrator    : " +
           joinTypes(Src::Integration::TimeIntegratorRegistry::getInstance()
                         .getRegisteredTypes()));
  LOG_INFO("  FieldRegistry     : " +
           joinTypes(PDCommon::Field::FieldRegistry::getInstance().getRegisteredTypes()));
  LOG_INFO("  PhysicsFields     : " +
           joinTypes(PDCommon::Field::PhysicsFieldRegistry::getInstance().getRegisteredTypes()));
  LOG_INFO("  BCRegistry        : " +
           joinTypes(PDCommon::BC::BCRegistry::getInstance().getRegisteredTypes()));
  LOG_INFO("  MaterialRegistry  : " +
           joinTypes(PDCommon::Material::MaterialRegistry::getInstance().getRegisteredTypes()));
  LOG_INFO("  KernelRegistry    : " +
           joinTypes(PDCommon::Kernel::KernelRegistry::getInstance().getRegisteredTypes()));
  LOG_INFO("  EngineRegistry    : " +
           joinTypes(Src::Engine::EngineRegistry::getInstance().getRegisteredTypes()));
  LOG_INFO("===============================================");
}

// ============================================================================
// 初始化前处理阶段 (Pre)
// ============================================================================
void PDEngine::Initialize(const std::string &yamlPath) {
  LOG_INFO("[PDEngine] Initializing PD simulation context...");
  yamlPath_ = yamlPath;

  YAML::Node config = YAML::LoadFile(yamlPath);

  // 使用静态门面层接口（分离自庞杂的 PDEngineInitializer）无缝调度各步骤构建
  Init::InitModel(pdContext_, config, yamlPath);
  Init::InitMaterial(pdContext_, config);
  Init::InitFields(pdContext_, config);
  Init::InitConditions(pdContext_, config);
  Init::InitNeighbors(pdContext_, config);
  Init::InitSolverComponents(config, integrator_, kernels_);

  LOG_INFO("[PDEngine] PD simulation context initialized successfully.");
}

// ============================================================================
// 时间循环推进阶段 (Run)
// ============================================================================
void PDEngine::Solve() {
  LOG_INFO("[PDEngine] Starting PD Engine Solve Phase...");

  if (!integrator_ || kernels_.empty()) {
    LOG_ERROR("[PDEngine] Solver components not initialized...");
    return;
  }
  
  // 定制挂钩的 lambda：当解算器跑出满足时间步的节点，会调用当前的 Output 输出至底层 IO
  auto outputCallback = [this](int step, double time) { this->Output(step, time); };

  // 全权委托执行端负责运行（传入多核集合）
  Run::ExecuteSolve(pdContext_, integrator_, kernels_, outputCallback);

  LOG_INFO("[PDEngine] PD Engine Solve Phase Finished.");
}

void PDEngine::Output() {
  Output(-1, 0.0);
}

// ============================================================================
// 导出处理阶段 (Post)
// ============================================================================
void PDEngine::Output(int step, double physicalTime) {
  // 屏蔽一切重负载脏活累活的 VTK 打包程序，交给静悄悄的 Post 大师执行
  Post::ExportVTK(pdContext_, yamlPath_, step, physicalTime);
}

} // namespace Src::Engine::Solvers::PD

REGISTER_ENGINE_TYPE(PD, []() {
  return std::make_unique<Src::Engine::Solvers::PD::PDEngine>();
});
