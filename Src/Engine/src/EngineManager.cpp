// ============================================================================
// EngineManager.cpp - 仿真引擎顶层调度器实现
// ============================================================================

#include "EngineManager.h"

#include "BCRegistry.h"
#include "EngineRegistry.h"
#include "FieldRegistry.h"
#include "KernelRegistry.h"
#include "Logger.h"
#include "MaterialRegistry.h"
#include "PhysicsFieldRegistry.h"
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
// Initialize: 解析 YAML → 创建求解器 → 逐一初始化
// ---------------------------------------------------------------------------
void EngineManager::Initialize(const std::string &yamlPath) {
  // 1. 打印编译期静态注册汇总（此时 Logger 文件已初始化）
  LOG_INFO("========== Registered Types Summary ===========");

  auto fieldTypes =
      PDCommon::Field::FieldRegistry::getInstance().getRegisteredTypes();
  std::string fieldStr;
  for (const auto &t : fieldTypes) {
    fieldStr += t + ", ";
  }
  if (!fieldStr.empty()) {
    fieldStr.erase(fieldStr.size() - 2);
  }
  LOG_INFO("  FieldRegistry     : " + fieldStr);

  auto physicsTypes =
      PDCommon::Field::PhysicsFieldRegistry::getInstance().getRegisteredTypes();
  std::string physicsStr;
  for (const auto &t : physicsTypes) {
    physicsStr += t + ", ";
  }
  if (!physicsStr.empty()) {
    physicsStr.erase(physicsStr.size() - 2);
  }
  LOG_INFO("  PhysicsFields     : " + physicsStr);

  auto bcTypes = PDCommon::BC::BCRegistry::getInstance().getRegisteredTypes();
  std::string bcStr;
  for (const auto &t : bcTypes) {
    bcStr += t + ", ";
  }
  if (!bcStr.empty()) {
    bcStr.erase(bcStr.size() - 2);
  }
  LOG_INFO("  BCRegistry        : " + bcStr);

  auto matTypes =
      PDCommon::Material::MaterialRegistry::getInstance().getRegisteredTypes();
  std::string matStr;
  for (const auto &t : matTypes) {
    matStr += t + ", ";
  }
  if (!matStr.empty()) {
    matStr.erase(matStr.size() - 2);
  }
  LOG_INFO("  MaterialRegistry  : " + matStr);

  auto kernelTypes =
      PDCommon::Kernel::KernelRegistry::getInstance().getRegisteredTypes();
  std::string kernelStr;
  for (const auto &t : kernelTypes) {
    kernelStr += t + ", ";
  }
  if (!kernelStr.empty()) {
    kernelStr.erase(kernelStr.size() - 2);
  }
  LOG_INFO("  KernelRegistry    : " + kernelStr);

  auto solverTypes = EngineRegistry::getInstance().getRegisteredTypes();
  std::string solverStr;
  for (const auto &t : solverTypes) {
    solverStr += t + ", ";
  }
  if (!solverStr.empty()) {
    solverStr.erase(solverStr.size() - 2);
  }
  LOG_INFO("  EngineRegistry    : " + solverStr);

  LOG_INFO("===============================================");

  // 2. 解析 YAML 获取需要激活的求解器类型
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

  // 3. 根据 Solver.Engine 通过 Registry 创建引擎实例
  auto &registry = EngineRegistry::getInstance();

  if (registry.hasType(engineType)) {
    engines_.push_back(registry.create(engineType));
  } else {
    LOG_WARNING("[EngineManager] Engine type '" + engineType +
                "' not found in registry. No solver will be activated.");
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
// Solve: 遍历所有求解器执行求解
// ---------------------------------------------------------------------------
void EngineManager::Solve() {
  LOG_INFO("[EngineManager] Entering Main Solver Phase");
  for (auto &engine : engines_) {
    LOG_INFO("[EngineManager] Solving: " + engine->getName());
    engine->Solve();
  }
  LOG_INFO("[EngineManager] Main Solver Phase finished.");
}

// ---------------------------------------------------------------------------
// Output: 遍历所有求解器执行输出
// ---------------------------------------------------------------------------
void EngineManager::Output() {
  LOG_INFO("[EngineManager] Entering Post-processing Phase");
  for (auto &engine : engines_) {
    LOG_INFO("[EngineManager] Outputting: " + engine->getName());
    engine->Output();
  }
  LOG_INFO("[EngineManager] Post-processing Phase finished.");
}

} // namespace Src::Engine
