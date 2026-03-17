// ============================================================================
// PD_Condition.cpp - PD 边界条件与载荷初始化（从 SE_Condition.cpp 迁移）
// ============================================================================

#include "PDSolver.h"
#include "GrpdReader.h"
#include "Logger.h"
#include <cstdlib>
#include <string>
#include <yaml-cpp/yaml.h>

namespace Src::Solve {

void PDSolver::InitConditions() {
  auto &ctx = pdContext_;
  LOG_INFO("Entering Conditions Initialization Phase...");

  // =================================================================
  // 1. 解析 YAML 配置
  // =================================================================
  YAML::Node config;
  try {
    config = YAML::LoadFile(yamlPath_);
  } catch (const YAML::BadFile &e) {
    LOG_ERROR("Failed to load YAML file in InitConditions: " +
              std::string(e.what()));
    exit(EXIT_FAILURE);
  }

  // =================================================================
  // 2. 从 .grpd 文件的 *LOAD 段读取载荷并施加到物理场
  // =================================================================
  std::string grpdPath = "";
  if (config["Assembly"] && config["Assembly"]["OutputGrpd"]) {
    grpdPath = config["Assembly"]["OutputGrpd"].as<std::string>();
  } else {
    LOG_ERROR("[InitConditions] Key [Assembly][OutputGrpd] not found! "
              "Cannot read load data.");
    return;
  }

  LOG_INFO("[InitConditions] Reading load data from .grpd file: " + grpdPath);

  if (!PDCommon::IO::GrpdReader::readLoads(grpdPath, ctx)) {
    LOG_ERROR("[InitConditions] Failed to read loads from .grpd file!");
    return;
  }

  LOG_INFO("[InitConditions] Boundary conditions initialization complete.");
}

} // namespace Src::Solve
