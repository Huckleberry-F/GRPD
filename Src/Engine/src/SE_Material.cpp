// ==============================================================================
// SE_Material.cpp
// Responsibility:
// 1. Parse 'Materials' from PD.yaml
// 2. Instantiate materials via MaterialRegistry
// 3. Assign material pointers to particles based on MatID
// ==============================================================================

#include "Logger.h"
#include "MaterialRegistry.h"
#include "SolverEngine.h"
#include <yaml-cpp/yaml.h>

namespace GRPD::Engine {

void SolverEngine::InitMaterialPD() {
  auto &ctx = this->pdContext_;
  LOG_INFO("Entering Material Initialization Phase...");

  std::string yamlPath = "../../Input/PD.yaml";
  YAML::Node config;
  try {
    config = YAML::LoadFile(yamlPath);
  } catch (const YAML::BadFile &e) {
    LOG_ERROR("Failed to load YAML file in InitMaterialPD: " +
              std::string(e.what()));
    exit(EXIT_FAILURE);
  }

  // 1. Create Materials and add to Manager
  auto &matManager = ctx.getMaterialManager();
  std::unordered_map<int, GRPD::Material::Material *> idToMatMap;

  // 获取材料配置节点 (支持单数 Material 或 复数 Materials)
  YAML::Node materialsNode;
  if (config["Materials"]) {
    materialsNode = config["Materials"];
  } else if (config["Material"]) {
    materialsNode = config["Material"];
  }

  if (materialsNode) {
    // 统一转换为列表处理：如果不是序列，就包装成只含一个元素的序列
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
            GRPD::Material::MaterialRegistry::getInstance().createMaterial(
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

  // 2. Assign Material Pointers to Particles
  LOG_INFO("Assigning material pointers to particles...");
  auto &particleArray = ctx.getParticleManager().getAllParticles();
  int assignCount = 0;

  for (auto &p : particleArray) {
    int matId = p.getMatId();

    GRPD::Material::Material *assignedMat = nullptr;
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

} // namespace GRPD::Engine