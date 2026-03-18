// ============================================================================
// InitSolverComponents.cpp - 从 YAML 创建 L1 (TimeIntegrator) + L2 (PDKernel)
// ============================================================================

#include "PDSolver.h"
#include "ExplicitEuler.h"
#include "Logger.h"
#include "NOSB_T.h"
#include <yaml-cpp/yaml.h>

namespace Src::Solve {

void PDSolver::InitSolverComponents() {
  LOG_INFO("[PDSolver] Creating solver components (L1 + L2)...");

  // =================================================================
  // 1. 解析 YAML 配置
  // =================================================================
  YAML::Node config = YAML::LoadFile(yamlPath_);

  // 求解器参数
  if (config["Solver"]) {
    auto solverNode = config["Solver"];
    if (solverNode["TimeStep_dt"])
      solverConfig_.dt = solverNode["TimeStep_dt"].as<double>();
    if (solverNode["TotalTime"])
      solverConfig_.totalTime = solverNode["TotalTime"].as<double>();
    if (solverNode["OutputInterval"])
      solverConfig_.outputInterval = solverNode["OutputInterval"].as<int>();
  }

  // 读取算法类型和 PD 理论类型
  std::string algorithm = "Explicit"; // 默认显式
  std::string pdType = "NOSB";        // 默认 NOSB
  std::string physics = "Thermal";    // 默认热传导

  if (config["Solver"]) {
    auto solverNode = config["Solver"];
    if (solverNode["Algorithm"])
      algorithm = solverNode["Algorithm"].as<std::string>();
    if (solverNode["PDType"])
      pdType = solverNode["PDType"].as<std::string>();
    if (solverNode["Physics"])
      physics = solverNode["Physics"].as<std::string>();
    // 兼容旧配置：如果 Type 字段存在且 Physics 字段不存在
    if (!solverNode["Physics"] && solverNode["Type"])
      physics = solverNode["Type"].as<std::string>();
  }

  LOG_INFO("[PDSolver] Algorithm: " + algorithm + ", PDType: " + pdType +
           ", Physics: " + physics);

  // =================================================================
  // 2. 创建 L1: TimeIntegrator
  // =================================================================
  if (algorithm == "Explicit") {
    integrator_ = std::make_unique<ExplicitEuler>();
  } else {
    LOG_ERROR("[PDSolver] Unknown algorithm type: '" + algorithm +
              "'. Supported: Explicit");
    integrator_ = std::make_unique<ExplicitEuler>(); // fallback
  }

  // =================================================================
  // 3. 创建 L2: PDKernel
  // =================================================================
  std::string kernelKey = pdType + "_" + physics; // 如 "NOSB_Thermal"

  if (kernelKey == "NOSB_Thermal") {
    kernel_ = std::make_unique<NOSB_T>();
  } else {
    LOG_ERROR("[PDSolver] Unknown kernel type: '" + kernelKey +
              "'. Supported: NOSB_Thermal");
    kernel_ = std::make_unique<NOSB_T>(); // fallback
  }

  LOG_INFO("[PDSolver] L1: " + integrator_->getName() +
           ", L2: " + kernelKey + " — components ready.");
}

} // namespace Src::Solve
