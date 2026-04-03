#include "IOManager.h"
#include "Logger.h"
#include "MeshData.h"
#include "PDEnginePre.h"
#include "ParticleManager.h"
#include "ReaderRegistry.h"
#include <cstdlib>
#include <filesystem>


namespace Src::Engine::Solvers::PD::Init {

// ============================================================================
// 步骤 1：初始化几何模型及网格节点解析
// 由 IOManager 定位 grpd 文件；若不存在则呼叫 Python 生成引擎生成该文件；
// 依据后缀多态调动对应的 ReaderRegistry 将拓扑结构吸入体系并映射到 Particle。
// ============================================================================
void InitModel(PDCommon::Core::PDContext &ctx, const YAML::Node &config,
               const std::string &yamlPath) {
  LOG_INFO("[InitModel] ==================================================");
  LOG_INFO("[InitModel] Model pre-processing...");
  LOG_INFO("[InitModel] ==================================================");

  auto &ioMgr = PDCommon::IO::IOManager::getInstance();
  std::string grpdPath = ioMgr.getGrpdPath().string();
  std::string currentModelName = ioMgr.getModelName();
  LOG_INFO("[InitModel] grpd path: " + grpdPath);
  LOG_INFO("[InitModel] model name: " + currentModelName);

  if (std::filesystem::exists(grpdPath)) {
    LOG_INFO("[InitModel] Found existing .grpd mesh file at: " + grpdPath +
             ". Skipping generation.");
  } else {
    LOG_INFO(
        "[InitModel] Calling Python pre-processor to generate .grpd mesh...");

    auto &ioMgr = PDCommon::IO::IOManager::getInstance();
    std::filesystem::path scriptPath = ioMgr.getScriptPath("generate_model.py");

    std::string pythonCommand =
        "python \"" + scriptPath.string() + "\" \"" + yamlPath + "\"";
    int pyStatus = std::system(pythonCommand.c_str());

    if (pyStatus != 0) {
      LOG_ERROR("Python pre-processor failed! Please check if python is "
                "installed and paths are correct.");
      LOG_ERROR("Exit code: " + std::to_string(pyStatus));
      exit(EXIT_FAILURE);
    }

    LOG_INFO("[InitModel] Mesh generated successfully by Python.");
  }

  ctx.setName(currentModelName);
  LOG_INFO(std::string("[Context Manager] Formalized model name to: ") +
           ctx.getName());

  try {
    if (config["Parts"] && config["Parts"].IsSequence() &&
        config["Parts"].size() > 0) {
      int dim = config["Parts"][0]["Dimension"]
                    ? config["Parts"][0]["Dimension"].as<int>()
                    : 3;
      ctx.setDimension(dim);
      LOG_INFO("[Context Manager] Model dimension set to: " +
               std::to_string(dim) + "D");
    }
  } catch (const YAML::Exception &e) {
    LOG_WARNING("[Context Manager] Failed to parse Dimension from YAML: " +
                std::string(e.what()) + ". Defaulting to 3D.");
    ctx.setDimension(3);
  }

  auto &manager = ctx.getParticleManager();

  // 依靠 ReaderRegistry 反射构造支持多格式文件 (默认是 .grpd 的二进制专用
  // Reader)
  namespace fs = std::filesystem;
  std::string ext = fs::path(grpdPath).extension().string();
  auto reader = PDCommon::IO::ReaderRegistry::getInstance().createReader(
      ext, currentModelName);

  if (!reader) {
    LOG_ERROR("[InitModel] No reader registered for extension: " + ext);
    exit(EXIT_FAILURE);
  }

  LOG_INFO("[InitModel] Reading mesh file: " + grpdPath + " (reader: " + ext +
           ")");

  if (!reader->read(grpdPath)) {
    LOG_ERROR("[InitModel] Reader failed for file: " + grpdPath);
    exit(EXIT_FAILURE);
  }

  const auto &meshData = reader->getMeshData();
  size_t numPts = meshData.nodeIDs.size();

  for (size_t i = 0; i < numPts; ++i) {
    manager.addParticle(meshData.nodeIDs[i], meshData.partIDs[i],
                        meshData.matIDs[i], meshData.coords[i * 3 + 0],
                        meshData.coords[i * 3 + 1], meshData.coords[i * 3 + 2],
                        meshData.volumes[i]);
  }

  LOG_INFO("[InitModel] Total particles loaded for model " + currentModelName +
           ": " + std::to_string(manager.getTotalParticles()));
}

} // namespace Src::Engine::Solvers::PD::Init
