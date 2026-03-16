// ============================================================================
// PD_Field.cpp - PD 物理场与状态变量注册（从 SE_Field.cpp 迁移）
// ============================================================================

#include "PDSolver.h"
#include "GrpdReader.h"
#include "Logger.h"
#include "PhysicsFieldRegistry.h"
#include <cstdlib>
#include <string>
#include <yaml-cpp/yaml.h>

namespace Src::Solve {

void PDSolver::InitFields() {
  auto &ctx = pdContext_;
  LOG_INFO("Entering Field Registration Phase...");

  // =================================================================
  // 1. 解析 YAML 配置，获取 Solver.Type
  // =================================================================
  YAML::Node config;
  try {
    config = YAML::LoadFile(yamlPath_);
  } catch (const YAML::BadFile &e) {
    LOG_ERROR("Failed to load YAML file in InitFields: " +
              std::string(e.what()));
    exit(EXIT_FAILURE);
  }

  std::string solverType = "";
  if (config["Solver"] && config["Solver"]["Type"]) {
    solverType = config["Solver"]["Type"].as<std::string>();
  }

  auto &fieldManager = ctx.getFieldManager();

  // =================================================================
  // 2. 通过 PhysicsFieldRegistry 多态注册核心物理场
  //    支持组合类型（如 "ThermalMechanical"）
  // =================================================================
  auto &registry = PDCommon::Field::PhysicsFieldRegistry::getInstance();

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
  // 3. 注册粒子基础几何场 (Coords, Volume, PartID 等)
  // =================================================================
  PDCommon::IO::GrpdReader::ensureParticleFields(fieldManager);

  // =================================================================
  // 4. 状态变量池 (SDV Pool) 初始化
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
  // 5. 遍历所有材料，执行材料私有状态变量场注册
  // =================================================================
  for (const auto &[matName, matPtr] : matManager.getMaterials()) {
    if (matPtr) {
      matPtr->allocateStateVariables(fieldManager);
    }
  }

  // =================================================================
  // 6. 统一分配内存，并从 ParticleManager 填充数据
  // =================================================================
  size_t numParticles = ctx.getParticleManager().getTotalParticles();
  fieldManager.resizeAll(numParticles);

  if (!PDCommon::IO::GrpdReader::populateParticleFields(
          ctx.getParticleManager(), fieldManager)) {
    LOG_ERROR("[InitFields] Failed to populate particle fields from "
              "ParticleManager.");
    exit(EXIT_FAILURE);
  }

  LOG_INFO("[InitFields] All fields allocated for " +
           std::to_string(numParticles) + " particles. Total fields: " +
           std::to_string(fieldManager.getFieldCount()) + ".");
}

} // namespace Src::Solve
