// ============================================================================
// SE_Initialize.cpp - 边界条件与载荷初始化
// 责任：
// 1. 从 .grpd 文件的 *LOAD 段读取载荷数据，施加到物理场
// 注意：物理场的注册与内存分配已由 SE_Field.cpp (InitFieldsPD) 完成
// ============================================================================

#include "GrpdReader.h"
#include "Logger.h"
#include "SolverEngine.h"
#include <cstdlib>
#include <filesystem>
#include <string>
#include <yaml-cpp/yaml.h>

namespace GRPD::Engine {

void SolverEngine::InitConditionsPD() {
  auto &ctx = this->pdContext_;
  LOG_INFO("Entering Conditions Initialization Phase...");

  // =================================================================
  // 1. 解析 YAML 配置
  // =================================================================
  std::string yamlPath = "../../Input/PD.yaml";
  YAML::Node config;
  try {
    config = YAML::LoadFile(yamlPath);
  } catch (const YAML::BadFile &e) {
    LOG_ERROR("Failed to load YAML file in InitConditionsPD: " +
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

  if (!GRPD::IO::GrpdReader::readLoads(grpdPath, ctx)) {
    LOG_ERROR("[InitConditions] Failed to read loads from .grpd file!");
    return;
  }

  LOG_INFO("[InitConditions] Boundary conditions initialization complete.");
}

} // namespace GRPD::Engine