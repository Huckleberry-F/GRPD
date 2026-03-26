#include "PDEnginePre.h"
#include "Logger.h"
#include "Material.h"
#include "MaterialManager.h"
#include "MaterialRegistry.h"
#include "Particle.h"
#include "ParticleManager.h"
#include <string>
#include <unordered_map>
#include <vector>

namespace Src::Engine::Solvers::PD::Init {

// ============================================================================
// 步骤 2：初始化本构模型池及分配机制
// 依次读取 YAML 内所有 Material 对象信息，创建对应的注册表内多态实例。
// 然后将其安全且去重地指针化赋予每个对应的 Particle。
// ============================================================================
void InitMaterial(PDCommon::Core::PDContext &ctx,
                  const YAML::Node &config) {
  LOG_INFO("Entering Material Initialization Phase...");

  auto &matManager = ctx.getMaterialManager();
  std::unordered_map<int, PDCommon::Material::Material *> idToMatMap;

  YAML::Node materialsNode;
  if (config["Materials"]) {
    materialsNode = config["Materials"];
  } else if (config["Material"]) {
    materialsNode = config["Material"];
  }

  if (materialsNode) {
    std::vector<YAML::Node> nodes;
    if (materialsNode.IsSequence()) {
      for (const auto &n : materialsNode) {
        nodes.push_back(n);
      }
    } else {
      nodes.push_back(materialsNode);
    }

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

        auto matPtr =
            PDCommon::Material::MaterialRegistry::getInstance().createMaterial(
                type, keyName);

        if (matPtr) {
          matPtr->initialize(matNode);
          matPtr->setName(keyName);
          matPtr->setMatId(matId);

          idToMatMap[matId] = matPtr.get();
          matManager.addMaterial(std::move(matPtr));

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

} // namespace Src::Engine::Solvers::PD::Init
