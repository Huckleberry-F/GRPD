// ============================================================================
// EngineManager.cpp - 仿真引擎顶层调度器实现
// 职责：解析 YAML 选择引擎 → 创建引擎 → 委派生命周期调用
// 不包含任何具体物理模块（PD/FEM）的头文件或逻辑
// ============================================================================

#include "EngineManager.h"

#include "EngineRegistry.h"
#include "Logger.h"
#include <yaml-cpp/yaml.h>

namespace Src::Engine {

// ---------------------------------------------------------------------------
// 构造函数
// ---------------------------------------------------------------------------
EngineManager::EngineManager() {
  LOG_INFO("==================================================");
  LOG_INFO("   GRPD Solver Engine Initialized                 ");
  LOG_INFO("==================================================");
}

// ---------------------------------------------------------------------------
// Setup: 解析 YAML → 创建求解器 → 逐一初始化
// ---------------------------------------------------------------------------
void EngineManager::Setup(const std::string &yamlPath) {
  // 1. 解析 YAML 获取需要激活的求解器类型
  YAML::Node config;
  try {
    config = YAML::LoadFile(yamlPath);
  } catch (const YAML::BadFile &e) {
    LOG_ERROR("[EngineManager] Failed to load YAML: " + std::string(e.what()));
    return;
  }

  // 读取引擎类型: Solver.Engine (如 "PD", "FEM")
  // 注意: Solver.Type (如 "Thermal") 是物理类型，由具体 Engine 子类内部处理
  std::string engineType = "PD"; // 默认使用 PD 引擎
  if (config["Solver"] && config["Solver"]["Engine"]) {
    engineType = config["Solver"]["Engine"].as<std::string>();
  }

  // 2. 根据 Solver.Engine 通过 Registry 创建引擎实例
  auto &registry = EngineRegistry::getInstance();

  if (registry.hasType(engineType)) {
    engines_.push_back(registry.create(engineType));
  } else {
    LOG_WARNING("[EngineManager] Engine type '" + engineType +
                "' not found in registry. No solver will be activated.");
  }

  // 3. 打印各引擎注册表信息（由子类多态实现）
  for (auto &engine : engines_) {
    engine->printRegistrySummary();
  }

  // 4. 逐一初始化所有求解器
  for (auto &engine : engines_) {
    LOG_INFO("[EngineManager] Initializing solver: " + engine->getName());
    engine->Initialize(yamlPath);
  }

  LOG_INFO("[EngineManager] All solvers initialized. Total: " +
           std::to_string(engines_.size()));
}

// ---------------------------------------------------------------------------
// RunAll: 遍历所有求解器执行求解
// ---------------------------------------------------------------------------
void EngineManager::RunAll() {
  LOG_INFO("[EngineManager] Entering Main Solver Phase");
  for (auto &engine : engines_) {
    LOG_INFO("[EngineManager] Solving: " + engine->getName());
    engine->Solve();
  }
  LOG_INFO("[EngineManager] Main Solver Phase finished.");
}

// ---------------------------------------------------------------------------
// ExportAll: 遍历所有求解器执行输出
// ---------------------------------------------------------------------------
void EngineManager::ExportAll() {
  LOG_INFO("[EngineManager] Entering Post-processing Phase");
  for (auto &engine : engines_) {
    LOG_INFO("[EngineManager] Outputting: " + engine->getName());
    engine->Output();
  }
  LOG_INFO("[EngineManager] Post-processing Phase finished.");
}

} // namespace Src::Engine
