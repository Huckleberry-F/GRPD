// ============================================================================
// Model.cpp - Model pre-processing
// Responsibility: Generate the initial mesh and save it to a .grpd file and
// send to ParticleManager
// ============================================================================
#include "GrpdReader.h"
#include "Logger.h"
#include "PDSimulater.h"
#include "SolverEngine.h"
#include <cstdlib>
#include <filesystem>
#include <string>
#include <yaml-cpp/yaml.h>

namespace GRPD::Engine {

void SolverEngine::InitModelPD() {
  auto &ctx = this->pdContext_;

  // =================================================================
  // Generate the initial mesh and save it to a .grpd file
  // =================================================================
  LOG_INFO("Model pre-processing...");

  LOG_INFO("Calling Python pre-processor to generate .grpd mesh...");

  std::string pythonCommand =
      "python ../../Input/generate_model.py ../../Input/PD.yaml";

  // Execute the command via std::system and capture the return value
  int pyStatus = std::system(pythonCommand.c_str());

  // Check if the Python script executed successfully (0 means success)
  if (pyStatus != 0) {
    LOG_ERROR("Python pre-processor failed! Please check if python is "
              "installed and paths are correct.");

    LOG_ERROR("Exit code: " + std::to_string(pyStatus));

    exit(EXIT_FAILURE); // Terminate the program immediately
  }

  LOG_INFO("Mesh generated successfully by Python.");

  // =================================================================
  // Parse YAML to get OutputGrpd path and derive model name
  // =================================================================
  LOG_INFO("Parsing PD.yaml to extract model configuration...");
  std::string yamlPath = "../../Input/PD.yaml";
  std::string grpdPath = "";
  std::string currentModelName = "";

  try {
    YAML::Node config = YAML::LoadFile(yamlPath);

    // 1. 采用版本 B 的高防御力：精准检查节点是否存在
    if (config["Assembly"] && config["Assembly"]["OutputGrpd"]) {
      grpdPath = config["Assembly"]["OutputGrpd"].as<std::string>();

      // 2. 采用版本 A 的极简优雅：使用 C++17 filesystem 提取文件名
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
  // Define current model context (Replacing global registry with instance)
  // =================================================================
  ctx.setName(currentModelName);
  LOG_INFO(std::string("[Context Manager] Formalized model name to: ") +
           ctx.getName());
  auto &manager = ctx.getParticleManager();

  // =================================================================
  // Read .grpd file and populate the specific particle manager
  // =================================================================
  LOG_INFO("Reading .grpd model file: " + grpdPath + " ...");

  if (!GRPD::IO::GrpdReader::read(grpdPath, manager)) {
    LOG_ERROR("Failed to read .grpd file!");
    exit(EXIT_FAILURE);
  }

  // =================================================================
  // Read done and print the total number of particles
  // =================================================================
  LOG_INFO("Total particles loaded for model " + currentModelName + ": " +
           std::to_string(manager.getTotalParticles()));

  // =================================================================
  // Verification: print info for the first 5 particles
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

} // namespace GRPD::Engine
