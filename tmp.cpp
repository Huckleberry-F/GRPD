// ============================================================================
// PDEngineInitializer.cpp - PD 姹傝В寮曟搸鍚勭粍浠跺垵濮嬪寲瀹炵幇
// 鏁村悎浜嗘ā鍨嬪姞杞姐€佹潗鏂欏垎閰嶃€佺墿鐞嗗満娉ㄥ唽銆佽竟鐣屾潯浠跺姞杞姐€侀偦鍩熸悳绱㈠強缁勪欢鍒濆鍖?// ============================================================================

#include "PDEngineInitializer.h"
#include "ExplicitEuler.h"
#include "FieldManager.h"
#include "FieldRegistry.h"
#include "MeshData.h"
#include "BCManager.h"
#include "BCRegistry.h"
#include "IOManager.h"
#include "ReaderRegistry.h"
#include "KernelRegistry.h"
#include "Logger.h"
#include "Material.h"
#include "MaterialManager.h"
#include "MaterialRegistry.h"
#include "NeighborList.h"
#include "Particle.h"
#include "ParticleManager.h"
#include "PhysicsFieldRegistry.h"
#include <cstdlib>
#include "TimeIntegratorRegistry.h"
#include <filesystem>
#include <memory>
#include <string>
#include <map>
#include <yaml-cpp/yaml.h>

namespace Src::Engine::Solvers::PD {

// ============================================================================
// 1. 鍒濆鍖栨ā鍨嬶細璋冪敤 Python 鑴氭湰鐢熸垚缃戞牸骞惰鍙?// ============================================================================
void PDEngineInitializer::InitModel(PDCommon::Core::PDContext &ctx,
                                    const YAML::Node &config,
                                    const std::string &yamlPath) {
  LOG_INFO("Model pre-processing...");

  // 閫氳繃 IOManager 鑷姩鎺ㄥ .grpd 璺緞鍜屾ā鍨嬪悕锛堟棤闇€ YAML 涓寚瀹?OutputGrpd锛?  auto &ioMgr = PDCommon::IO::IOManager::getInstance();
  std::string grpdPath = ioMgr.getGrpdPath().string();
  std::string currentModelName = ioMgr.getModelName();
  LOG_INFO("[InitModel] grpd path: " + grpdPath);
  LOG_INFO("[InitModel] model name: " + currentModelName);

  if (std::filesystem::exists(grpdPath)) {
    LOG_INFO("Found existing .grpd mesh file at: " + grpdPath +
             ". Skipping generation.");
  } else {
    LOG_INFO("Calling Python pre-processor to generate .grpd mesh...");

    // 閫氳繃 IOManager 浠庤蒋浠跺畨瑁呯洰褰曞畾浣?generate_model.py
    auto &ioMgr = PDCommon::IO::IOManager::getInstance();
    std::filesystem::path scriptPath =
        ioMgr.getScriptPath("generate_model.py");

    std::string pythonCommand =
        "python \"" + scriptPath.string() + "\" \"" + yamlPath + "\"";
    int pyStatus = std::system(pythonCommand.c_str());

    if (pyStatus != 0) {
      LOG_ERROR("Python pre-processor failed! Please check if python is "
                "installed and paths are correct.");
      LOG_ERROR("Exit code: " + std::to_string(pyStatus));
      exit(EXIT_FAILURE);
    }

    LOG_INFO("Mesh generated successfully by Python.");
  }

  // 璁剧疆妯″瀷涓婁笅鏂囧悕绉?  ctx.setName(currentModelName);
  LOG_INFO(std::string("[Context Manager] Formalized model name to: ") +
           ctx.getName());

  // 浠?YAML 瑙ｆ瀽妯″瀷缁村害 (Parts[0].Dimension)锛岄粯璁?3D
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
  }

  auto &manager = ctx.getParticleManager();

  // ================================================================
  // 澶氭€佽鍙栵細閫氳繃 ReaderRegistry 鎸夋枃浠跺悗缂€鍚嶈嚜鍔ㄨ皟搴﹁鍙栧櫒
  // 鏂板鏍煎紡鍙渶娉ㄥ唽瀵瑰簲 Reader锛屾澶勯浂淇敼
  // ================================================================
  namespace fs = std::filesystem;
  std::string ext = fs::path(grpdPath).extension().string();
  auto reader = PDCommon::IO::ReaderRegistry::getInstance().createReader(
      ext, currentModelName);

  if (!reader) {
    LOG_ERROR("[InitModel] No reader registered for extension: " + ext);
    exit(EXIT_FAILURE);
  }

  LOG_INFO("[InitModel] Reading mesh file: " + grpdPath +
           " (reader: " + ext + ")");

  if (!reader->read(grpdPath)) {
    LOG_ERROR("[InitModel] Reader failed for file: " + grpdPath);
    exit(EXIT_FAILURE);
  }

  // 浠?MeshData 涓棿缁撴瀯濉厖 ParticleManager
  const auto &meshData = reader->getMeshData();
  size_t numPts = meshData.nodeIDs.size();

  for (size_t i = 0; i < numPts; ++i) {
    manager.addParticle(meshData.nodeIDs[i], meshData.partIDs[i],
                        meshData.matIDs[i], meshData.coords[i * 3 + 0],
                        meshData.coords[i * 3 + 1],
                        meshData.coords[i * 3 + 2], meshData.volumes[i]);
  }

  LOG_INFO("[InitModel] Total particles loaded for model " +
           currentModelName + ": " +
           std::to_string(manager.getTotalParticles()));

  // 楠岃瘉锛氭墦鍗板墠 5 涓矑瀛愪俊鎭?  size_t previewCount =
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

// ============================================================================
// 2. 鍒濆鍖栨潗鏂欙細浠?YAML 璇诲彇鏉愭枡瀹氫箟骞跺垎閰嶇粰绮掑瓙
// ============================================================================
void PDEngineInitializer::InitMaterial(PDCommon::Core::PDContext &ctx,
                                       const YAML::Node &config) {
  LOG_INFO("Entering Material Initialization Phase...");

  auto &matManager = ctx.getMaterialManager();
  std::unordered_map<int, PDCommon::Material::Material *> idToMatMap;

  // 鑾峰彇鏉愭枡閰嶇疆鑺傜偣 (鏀寔鍗曟暟 Material 鎴栧鏁?Materials)
  YAML::Node materialsNode;
  if (config["Materials"]) {
    materialsNode = config["Materials"];
  } else if (config["Material"]) {
    materialsNode = config["Material"];
  }

  if (materialsNode) {
    // 缁熶竴杞崲涓哄垪琛ㄥ鐞?    std::vector<YAML::Node> nodes;
    if (materialsNode.IsSequence()) {
      for (const auto &n : materialsNode) {
        nodes.push_back(n);
      }
    } else {
      nodes.push_back(materialsNode);
    }

    // 寰幆瀹炰緥鍖栨墍鏈夊畾涔夌殑鏉愭枡
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

        // 璋冪敤宸ュ巶妯″紡鍒涘缓瀹炰緥
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

  // 灏嗘潗鏂欐寚閽堝垎閰嶇粰鍚勪釜绮掑瓙
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

// ============================================================================
// 3. 鍒濆鍖栫墿鐞嗗満锛氭牴鎹?Solver.Type 娉ㄥ唽鏍稿績鍦哄強鏉愭枡鐘舵€佸彉閲忓満
// ============================================================================
void PDEngineInitializer::InitFields(PDCommon::Core::PDContext &ctx,
                                     const YAML::Node &config) {
  LOG_INFO("Entering Field Registration Phase...");

  std::string solverType = "";
  if (config["Solver"] && config["Solver"]["Type"]) {
    solverType = config["Solver"]["Type"].as<std::string>();
  }

  auto &fieldManager = ctx.getFieldManager();

  // 閫氳繃 PhysicsFieldRegistry 澶氭€佹敞鍐屾牳蹇冪墿鐞嗗満 (鏀寔缁勫悎绫诲瀷濡?  // "ThermalMechanical")
  auto &registry = PDCommon::Field::PhysicsFieldRegistry::getInstance();
  bool anyMatch = false;

  for (const auto &registeredType : registry.getRegisteredTypes()) {
    if (solverType.find(registeredType) != std::string::npos) {
      auto fields = registry.create(registeredType);
      if (fields) {
        fields->registerFields(fieldManager);
        anyMatch = true;
        LOG_INFO("[InitFields] Auto-registered PhysicsFields module: " +
                 registeredType);
      }
    }
  }

  if (!anyMatch) {
    LOG_WARNING("[InitFields] Solver type '" + solverType +
                "' did not match any registered PhysicsFields types.");
  }

  // 娉ㄥ唽绮掑瓙鍩虹鍑犱綍鍦?(Coords, Volume, PartID 绛?
  PDCommon::IO::MeshData::ensureParticleFields(fieldManager);

  // 鐘舵€佸彉閲忔睜 (SDV Pool) 鍒濆鍖?  auto &matManager = ctx.getMaterialManager();
  size_t maxSDVs = 0;
  for (const auto &[matName, matPtr] : matManager.getMaterials()) {
    if (matPtr) {
      maxSDVs = std::max(maxSDVs, matPtr->getNumStateVariables());
    }
  }

  if (maxSDVs > 0) {
    auto sdvField = PDCommon::Field::FieldRegistry::getInstance()
        .createField("DoubleField", "SDV", static_cast<int>(maxSDVs));
    fieldManager.addField(std::move(sdvField));
    LOG_INFO("[InitFields] SDV Pool registered with dimension: " +
             std::to_string(maxSDVs));
  }

  // 閬嶅巻鎵€鏈夋潗鏂欙紝鎵ц鏉愭枡绉佹湁鐘舵€佸彉閲忓満娉ㄥ唽
  for (const auto &[matName, matPtr] : matManager.getMaterials()) {
    if (matPtr) {
      matPtr->allocateStateVariables(fieldManager);
    }
  }

  // 缁熶竴鍒嗛厤鍐呭瓨锛屽苟浠?ParticleManager 濉厖鍑犱綍鏁版嵁
  size_t numParticles = ctx.getParticleManager().getTotalParticles();
  fieldManager.resizeAll(numParticles);

  if (!PDCommon::IO::MeshData::populateParticleFields(
          ctx.getParticleManager(), fieldManager)) {
    LOG_ERROR("[InitFields] Failed to populate particle fields from "
              "ParticleManager.");
    exit(EXIT_FAILURE);
  }

  LOG_INFO("[InitFields] All fields allocated for " +
           std::to_string(numParticles) + " particles. Total fields: " +
           std::to_string(fieldManager.getFieldCount()) + ".");
}

// ============================================================================
// 4. 鍒濆鍖栬竟鐣屾潯浠讹細閫氳繃 MeshReader 璇诲彇杞借嵎骞舵柦鍔?// ============================================================================
void PDEngineInitializer::InitConditions(PDCommon::Core::PDContext &ctx,
                                         const YAML::Node &config) {
  LOG_INFO("Entering Conditions Initialization Phase...");

  // 閫氳繃澶氭€?Reader 璇诲彇缃戞牸鏂囦欢锛堝寘鍚?*LOAD 娈碉級
  auto &ioMgr = PDCommon::IO::IOManager::getInstance();
  std::string grpdPath = ioMgr.getGrpdPath().string();
  namespace fs = std::filesystem;
  std::string ext = fs::path(grpdPath).extension().string();

  auto reader = PDCommon::IO::ReaderRegistry::getInstance().createReader(
      ext, "LoadReader");
  if (!reader || !reader->read(grpdPath)) {
    LOG_ERROR("[InitConditions] Failed to read loads from file: " + grpdPath);
    return;
  }

  // 浠?meshData_.loads 涓柦鍔犺竟鐣屾潯浠?  const auto &loads = reader->getMeshData().loads;
  auto &fieldManager = ctx.getFieldManager();
  auto &bcManager = ctx.getBCManager();

  std::map<std::string, int> bcStats;

  for (const auto &entry : loads) {
    std::string bcName = entry.type + "_BC" + std::to_string(entry.bcID) +
                         "_P" + std::to_string(entry.nodeID);

    auto bc =
        PDCommon::BC::BCRegistry::getInstance().createBC(entry.type, bcName);
    if (!bc) {
      LOG_WARNING("[InitConditions] Unknown BC type: " + entry.type);
      bcStats["Unknown"]++;
      continue;
    }

    std::vector<double> values = {entry.value};
    // initialize 鐢卞悇鑷殑鍏蜂綋绫诲瀷 (濡?HeatFluxBC) 鍐呴儴鍘昏幏鍙栬嚜宸变笓灞炵殑鐗╃悊鍦哄苟缂撳瓨鎹㈢畻绯绘暟
    bc->initialize(fieldManager, entry.nodeID, values);

    bcStats[entry.type]++;
    bcManager.addBC(std::move(bc));
  }

  // ==================== 銆愬熀浜庡鎬佺殑鍒濆鐘舵€佽祴鍊笺€?====================
  // 涓嶅啀纭紪鐮佹鏌?Temperature 鎴?Displacement銆?  // 閫氳繃缁熶竴璋冪敤 bcManager 椹卞姩澶氭€佷笅鐨?apply()銆?  // 1. applySources: 鍒濆鍖栧苟鎹㈢畻鐢变簬杈圭晫甯﹀叆鐨勭巼鍦?(濡?TempRate, HeatFlux, ForceDensity)
  // 2. applyConstraints: 鍒濆鍖栫害鏉熷満鐘舵€?(濡傝瀹?t=0 鏃剁殑浣嶇Щ杈圭晫鍜屾亽娓╄竟鐣?
  bcManager.applySources();
  bcManager.applyConstraints();
  // ====================================================================

  LOG_INFO("[InitConditions] Load statistics:");
  for (const auto &[type, count] : bcStats) {
      LOG_INFO("  - " + type + " : " + std::to_string(count));
  }

  LOG_INFO("[InitConditions] Boundary conditions initialization complete.");
}

// ============================================================================
// 5. 鍒濆鍖栭偦鍩燂細璇诲彇 Horizon锛屾瀯寤洪偦灞呭垪琛紝鐢熸垚 NeighborCount 鍜?BondDamage
// 閿満
// ============================================================================
void PDEngineInitializer::InitNeighbors(PDCommon::Core::PDContext &ctx,
                                        const YAML::Node &config) {
  LOG_INFO("[InitNeighbors] Entering Neighbor Search Phase...");

  // 浠?YAML 璇诲彇 Solver.Horizon
  double horizon = 0.0;
  if (config["Solver"] && config["Solver"]["Horizon"]) {
    horizon = config["Solver"]["Horizon"].as<double>();
  } else {
    LOG_ERROR("[InitNeighbors] Solver.Horizon not found in YAML! Please "
              "specify the horizon radius.");
    exit(EXIT_FAILURE);
  }

  LOG_INFO("[InitNeighbors] Horizon radius: " + std::to_string(horizon));

  // 鏋勫缓杩戦偦鍒楄〃锛堝熀浜?Cell List 绠楁硶锛?  auto &mgr = ctx.getParticleManager();
  ctx.createNeighborList(mgr, horizon);

  // 娉ㄥ唽 NeighborCount 鍦哄苟濉厖鏁版嵁
  auto &fieldManager = ctx.getFieldManager();
  auto &neighborList = ctx.getNeighborList();
  size_t numParticles = mgr.getTotalParticles();

  auto ncFieldNew = PDCommon::Field::FieldRegistry::getInstance()
      .createField("DoubleField", "NeighborCount", 1);
  fieldManager.addField(std::move(ncFieldNew));
  auto *ncField = fieldManager.getFieldAs<double>("NeighborCount");

  if (ncField) {
    ncField->resize(numParticles);
    double *ncPtr = ncField->dataPtr();

    for (size_t i = 0; i < numParticles; ++i) {
      ncPtr[i] = static_cast<double>(
          neighborList.getNeighborCount(static_cast<int>(i)));
    }
    LOG_INFO("[InitNeighbors] NeighborCount field populated for " +
             std::to_string(numParticles) + " particles.");
  }

  // 娉ㄥ唽 BondDamage 閿満锛堝垵濮嬪€?1.0 = 瀹屽ソ锛?  neighborList.registerBondField("BondDamage");
  double *dmgPtr = neighborList.getBondFieldPtr("BondDamage");
  size_t nBonds = neighborList.totalBonds();
  std::fill(dmgPtr, dmgPtr + nBonds, 1.0);
  LOG_INFO("[InitNeighbors] BondDamage field registered, " +
           std::to_string(nBonds) + " bonds initialized to 1.0");

  LOG_INFO("[InitNeighbors] Neighbor search phase complete.");
}

// ============================================================================
// 6. 鍒濆鍖栨牳蹇冩眰瑙ｇ粍浠讹細浠?YAML 鍒涘缓 L1 (鏃堕棿绉垎鍣? + L2 (PD 绌洪棿鏍?
// ============================================================================
void PDEngineInitializer::InitSolverComponents(
    const YAML::Node &config,
    std::unique_ptr<Src::Integration::TimeIntegrator> &integrator,
    std::unique_ptr<PDCommon::Kernel::PDKernel> &kernel) {
  LOG_INFO("[PDEngine] Creating solver components (L1 + L2)...");

  // 璇诲彇绠楁硶绫诲瀷鍜?PD 鐞嗚绫诲瀷
  std::string algorithm = "Explicit"; // 榛樿鏄惧紡闆嗘垚
  std::string pdType = "NOSB";        // 榛樿 NOSB 鎬佸熀
  std::string physics = "Thermal";    // 榛樿鐑紶瀵?
  if (config["Solver"]) {
    auto solverNode = config["Solver"];
    if (solverNode["Algorithm"])
      algorithm = solverNode["Algorithm"].as<std::string>();
    if (solverNode["PDType"])
      pdType = solverNode["PDType"].as<std::string>();
    if (solverNode["Physics"])
      physics = solverNode["Physics"].as<std::string>();

    // 鍏煎鏃ч厤缃細濡傛灉瀛樺湪 Type 浼樺厛浣滀负 Physics 鍚庡
    if (!solverNode["Physics"] && solverNode["Type"])
      physics = solverNode["Type"].as<std::string>();
  }

  LOG_INFO("[PDSolver] Algorithm: " + algorithm + ", PDType: " + pdType +
           ", Physics: " + physics);

  // 鍒涘缓 L1: 鏃堕棿鎺ㄨ繘绯荤粺 (TimeIntegrator) 鈥?鍙嶅皠宸ュ巶锛岄浂 if-else
  integrator = Src::Integration::TimeIntegratorRegistry::getInstance()
                   .create(algorithm);
  if (!integrator) {
    LOG_ERROR("[PDEngine] Unknown algorithm type: '" + algorithm +
              "'. Registered types: ");
    for (const auto &t : Src::Integration::TimeIntegratorRegistry::getInstance()
                             .getRegisteredTypes()) {
      LOG_ERROR("  - " + t);
    }
    exit(EXIT_FAILURE);
  }

  // 澶氭€侀厤缃細绉垎鍣ㄨ嚜宸变粠 YAML 涓彁鍙栧睘浜庤嚜宸辩殑鍙傛暟
  if (config["Solver"]) {
    integrator->configure(config["Solver"]);
  }

  // 鍒涘缓 L2: PD 绉垎鏍稿績 (PDKernel) 鈥?鍙嶅皠宸ュ巶锛岄浂 if-else
  std::string kernelKey = pdType + "_" + physics;

  kernel = PDCommon::Kernel::KernelRegistry::getInstance().create(kernelKey);
  if (!kernel) {
    LOG_ERROR("[PDEngine] Unknown kernel type: '" + kernelKey +
              "'. Registered types: ");
    for (const auto &t :
         PDCommon::Kernel::KernelRegistry::getInstance().getRegisteredTypes()) {
      LOG_ERROR("  - " + t);
    }
    exit(EXIT_FAILURE);
  }

  // 澶氭€侀厤缃細鍐呮牳鑷繁浠?YAML 涓彁鍙栧睘浜庤嚜宸卞鏃忕殑鍙傛暟
  if (config["Solver"]) {
    kernel->configure(config["Solver"]);
  }

  LOG_INFO("[PDEngine] L1: " + integrator->getName() + ", L2: " + kernelKey +
           " - components ready.");
}

} // namespace Src::Engine::Solvers::PD
