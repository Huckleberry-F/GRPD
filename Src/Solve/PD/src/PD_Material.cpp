// ============================================================================
// PD_Material.cpp - PD 材料初始化与分配（从 SE_Material.cpp 迁移）
// ============================================================================

#include "PDSolver.h"
#include "Logger.h"
#include "MaterialRegistry.h"
#include <yaml-cpp/yaml.h>

namespace Src::Solve {

void PDSolver::InitMaterial() {
  auto &ctx = pdContext_;
  LOG_INFO("Entering Material Initialization Phase...");

  YAML::Node config;
  try {
    config = YAML::LoadFile(yamlPath_);
  } catch (const YAML::BadFile &e) {
    LOG_ERROR("Failed to load YAML file in InitMaterial: " +
              std::string(e.what()));
    exit(EXIT_FAILURE);
  }

  // =================================================================
  // 1. 创建材料实例并加入 Manager
  // =================================================================
  auto &matManager = ctx.getMaterialManager();
  std::unordered_map<int, PDCommon::Material::Material *> idToMatMap;

  // 获取材料配置节点 (支持单数 Material 或 复数 Materials)
  YAML::Node materialsNode;
  if (config["Materials"]) {
    materialsNode = config["Materials"];
  } else if (config["Material"]) {
    materialsNode = config["Material"];
  }

  if (materialsNode) {
    // 统一转换为列表处理
    std::vector<YAML::Node> nodes;
    if (materialsNode.IsSequence()) {
      for (const auto &n : materialsNode) {
        nodes.push_back(n);
      }
    } else {
      nodes.push_back(materialsNode);
    }

    // 循环实例化所有定义的材料
    for (const auto &matNode : nodes) {
      try {
        int matId = matNode["MatID"] ? matNode["MatID"].as<int>() : 1;
        std::string type = matNode["Type"].as<std::string>();
        std::string name =
            matNode["Name"] ? matNode["Name"].as<std::string>() : "";

        LOG_INFO("Instantiating material [" + std::to_string(matId) +
                 "] of type: " + type);

        std::string keyName =
            name.empty() ? "Mat_" + std::to_string(matId) : name;

        // 利用工厂模式创建实例
        auto matPtr =
            PDCommon::Material::MaterialRegistry::getInstance().createMaterial(
                type, keyName);

        if (matPtr) {
          matPtr->initialize(matNode);
          matPtr->setName(keyName);
          matPtr->setMatId(matId);

          idToMatMap[matId] = matPtr.get();
          matManager.addMaterial(keyName, std::move(matPtr));

          LOG_INFO("Successfully registered material ID " +
                   std::to_string(matId) + " (" + keyName + ")");
        }

      } catch (const std::exception &e) {
        LOG_ERROR("Error parsing material entry: " + std::string(e.what()));
      }
    }
  } else {
    LOG_WARNING("No 'Material' or 'Materials' section found in PD.yaml!");
  }

  // =================================================================
  // 2. 将材料指针分配给粒子
  // =================================================================
  LOG_INFO("Assigning material pointers to particles...");
  auto &particleArray = ctx.getParticleManager().getAllParticles();
  int assignCount = 0;

  for (auto &p : particleArray) {
    int matId = p.getMatId();

    PDCommon::Material::Material *assignedMat = nullptr;
    auto it = idToMatMap.find(matId);
    if (it != idToMatMap.end()) {
      assignedMat = it->second;
    }

    if (assignedMat) {
      p.setMaterial(assignedMat);
      assignCount++;
    } else {
      LOG_ERROR("Particle ID " + std::to_string(p.getId()) +
                " requests MatID " + std::to_string(matId) +
                " but it was not defined in YAML!");
    }
  }

  LOG_INFO("Successfully assigned materials to " + std::to_string(assignCount) +
           " particles.");
}

} // namespace Src::Solve
