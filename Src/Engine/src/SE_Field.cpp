// ============================================================================
// SE_Field.cpp - 物理场与状态变量注册
// 责任：
// 1. 根据 Solver.Type 通过 PhysicsFieldRegistry 自动注册核心物理场
// 2. 遍历所有材料，让每个材料子类注册其私有的状态变量场
// 3. 统一为所有场分配内存
// ============================================================================

#include "Logger.h"
#include "PhysicsFieldRegistry.h"
#include "SolverEngine.h"
#include <cstdlib>
#include <string>
#include <yaml-cpp/yaml.h>

namespace GRPD::Engine {

void SolverEngine::InitFieldsPD() {
  auto &ctx = this->pdContext_;
  LOG_INFO("Entering Field Registration Phase...");

  // =================================================================
  // 1. 解析 YAML 配置，获取 Solver.Type
  // =================================================================
  std::string yamlPath = "../../Input/PD.yaml";
  YAML::Node config;
  try {
    config = YAML::LoadFile(yamlPath);
  } catch (const YAML::BadFile &e) {
    LOG_ERROR("Failed to load YAML file in InitFieldsPD: " +
              std::string(e.what()));
    exit(EXIT_FAILURE);
  }

  std::string solverType = "";
  if (config["Solver"] && config["Solver"]["Type"]) {
    solverType = config["Solver"]["Type"].as<std::string>();
  }

  auto &fieldManager = ctx.getFieldManager();

  // =================================================================
  // 2. 解析 Solver.Type，通过 PhysicsFieldRegistry 多态注册核心场
  //    支持组合类型（如 "ThermalMechanical" 同时激活热场和力学场）
  // =================================================================
  auto &registry = GRPD::Field::PhysicsFieldRegistry::getInstance();

  // 检查已注册的物理类型，逐一匹配
  bool anyMatch = false;

  if (solverType.find("Thermal") != std::string::npos &&
      registry.hasType("Thermal")) {
    auto thermalFields = registry.create("Thermal");
    thermalFields->registerFields(fieldManager);
    anyMatch = true;
  }

  if (solverType.find("Mechanical") != std::string::npos &&
      registry.hasType("Mechanical")) {
    auto mechFields = registry.create("Mechanical");
    mechFields->registerFields(fieldManager);
    anyMatch = true;
  }

  if (!anyMatch) {
    LOG_WARNING("[InitFields] Solver type '" + solverType +
                "' did not match any registered PhysicsFields types.");
  }

  // =================================================================
  // 3. 状态变量池 (SDV Pool) 初始化
  //    扫描所有材料，确定最大状态变量需求数，分配统一池
  // =================================================================
  auto &matManager = ctx.getMaterialManager();
  size_t maxSDVs = 0;
  for (const auto &[matName, matPtr] : matManager.getMaterials()) {
    if (matPtr) {
      maxSDVs = std::max(maxSDVs, matPtr->getNumStateVariables());
    }
  }

  if (maxSDVs > 0) {
    fieldManager.registerField("SDV", static_cast<int>(maxSDVs));
    LOG_INFO("[InitFields] SDV Pool registered with dimension: " +
             std::to_string(maxSDVs));
  }

  // =================================================================
  // 4. 遍历所有材料，执行辅助场注册 (用于非池化特殊需求)
  // =================================================================
  for (const auto &[matName, matPtr] : matManager.getMaterials()) {
    if (matPtr) {
      matPtr->allocateStateVariables(fieldManager);
    }
  }

  // =================================================================
  // 5. 统一为所有已注册场分配内存
  // =================================================================
  size_t numParticles = ctx.getParticleManager().getTotalParticles();
  fieldManager.resizeAll(numParticles);

  LOG_INFO("[InitFields] All fields allocated for " +
           std::to_string(numParticles) + " particles. Total fields: " +
           std::to_string(fieldManager.getFieldCount()) + ".");
}

} // namespace GRPD::Engine
