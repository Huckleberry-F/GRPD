// ============================================================================
// PD_Model.cpp - PD 模型生成与粒子加载（从 SE_Model.cpp 迁移）
// ============================================================================

#include "PDSolver.h"
#include "GrpdReader.h"
#include "Logger.h"
#include <cstdlib>
#include <filesystem>
#include <string>
#include <yaml-cpp/yaml.h>

namespace Src::Solve {

void PDSolver::InitModel() {
  auto &ctx = pdContext_;

  // =================================================================
  // 1. 调用 Python 预处理器生成 .grpd 网格文件
  // =================================================================
  LOG_INFO("Model pre-processing...");
  LOG_INFO("Calling Python pre-processor to generate .grpd mesh...");

  std::string pythonCommand =
      "python ../../Input/generate_model.py " + yamlPath_;

  int pyStatus = std::system(pythonCommand.c_str());

  if (pyStatus != 0) {
    LOG_ERROR("Python pre-processor failed! Please check if python is "
              "installed and paths are correct.");
    LOG_ERROR("Exit code: " + std::to_string(pyStatus));
    exit(EXIT_FAILURE);
  }

  LOG_INFO("Mesh generated successfully by Python.");

  // =================================================================
  // 2. 解析 YAML 获取 OutputGrpd 路径和模型名
  // =================================================================
  LOG_INFO("Parsing PD.yaml to extract model configuration...");
  std::string grpdPath = "";
  std::string currentModelName = "";

  try {
    YAML::Node config = YAML::LoadFile(yamlPath_);

    if (config["Assembly"] && config["Assembly"]["OutputGrpd"]) {
      grpdPath = config["Assembly"]["OutputGrpd"].as<std::string>();
      std::filesystem::path p(grpdPath);
      currentModelName = p.stem().string();
    } else {
      LOG_ERROR("Key [Assembly][OutputGrpd] not found in YAML file!");
      exit(EXIT_FAILURE);
    }
  } catch (const YAML::Exception &e) {
    LOG_ERROR("Failed to parse YAML file: " + std::string(e.what()));
    exit(EXIT_FAILURE);
  }

  // =================================================================
  // 3. 设置模型上下文名称
  // =================================================================
  ctx.setName(currentModelName);
  LOG_INFO(std::string("[Context Manager] Formalized model name to: ") +
           ctx.getName());
  auto &manager = ctx.getParticleManager();

  // =================================================================
  // 4. 读取 .grpd 文件并填充粒子管理器
  // =================================================================
  LOG_INFO("Reading .grpd model file: " + grpdPath + " ...");

  if (!PDCommon::IO::GrpdReader::read(grpdPath, manager)) {
    LOG_ERROR("Failed to read .grpd file!");
    exit(EXIT_FAILURE);
  }

  LOG_INFO("Total particles loaded for model " + currentModelName + ": " +
           std::to_string(manager.getTotalParticles()));

  // =================================================================
  // 5. 验证：打印前 5 个粒子信息
  // =================================================================
  size_t previewCount =
      std::min(static_cast<size_t>(5), manager.getTotalParticles());
  for (size_t i = 0; i < previewCount; i++) {
    const auto &p = manager.getParticle(static_cast<int>(i));
    LOG_INFO("  Particle[" + std::to_string(p.getId()) +
             "] Part=" + std::to_string(p.getPartId()) + " Pos=(" +
             std::to_string(p.getCoords()[0]) + ", " +
             std::to_string(p.getCoords()[1]) + ", " +
             std::to_string(p.getCoords()[2]) + ")" +
             " Vol=" + std::to_string(p.getVolume()));
  }
}

} // namespace Src::Solve
