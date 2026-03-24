// ============================================================================
// PDEngineInitializer.cpp - PD 求解引擎各组件初始化实现
// 整合了模型加载、材料分配、物理场注册、边界条件加载、邻域搜索及组件初始化
// ============================================================================

#include "PDEngineInitializer.h"
#include "ExplicitEuler.h"
#include "FieldManager.h"
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
#include <filesystem>
#include <memory>
#include <string>
#include <map>
#include <yaml-cpp/yaml.h>

namespace Src::Engine::Solvers::PD {

// ============================================================================
// 1. 初始化模型：调用 Python 脚本生成网格并读取
// ============================================================================
void PDEngineInitializer::InitModel(PDCommon::Core::PDContext &ctx,
                                    const std::string &yamlPath) {
  LOG_INFO("Model pre-processing...");

  // 通过 IOManager 自动推导 .grpd 路径和模型名（无需 YAML 中指定 OutputGrpd）
  auto &ioMgr = PDCommon::IO::IOManager::getInstance();
  std::string grpdPath = ioMgr.getGrpdPath().string();
  std::string currentModelName = ioMgr.getModelName();
  LOG_INFO("[InitModel] grpd path: " + grpdPath);
  LOG_INFO("[InitModel] model name: " + currentModelName);

  if (std::filesystem::exists(grpdPath)) {
    LOG_INFO("Found existing .grpd mesh file at: " + grpdPath +
             ". Skipping generation.");
  } else {
    LOG_INFO("Calling Python pre-processor to generate .grpd mesh...");

    // 通过 IOManager 从软件安装目录定位 generate_model.py
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

  // 设置模型上下文名称
  ctx.setName(currentModelName);
  LOG_INFO(std::string("[Context Manager] Formalized model name to: ") +
           ctx.getName());

  // 从 YAML 解析模型维度 (Parts[0].Dimension)，默认 3D
  try {
    YAML::Node config = YAML::LoadFile(yamlPath);
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
  // 多态读取：通过 ReaderRegistry 按文件后缀名自动调度读取器
  // 新增格式只需注册对应 Reader，此处零修改
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

  // 从 MeshData 中间结构填充 ParticleManager
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

  // 验证：打印前 5 个粒子信息
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

// ============================================================================
// 2. 初始化材料：从 YAML 读取材料定义并分配给粒子
// ============================================================================
void PDEngineInitializer::InitMaterial(PDCommon::Core::PDContext &ctx,
                                       const std::string &yamlPath) {
  LOG_INFO("Entering Material Initialization Phase...");

  YAML::Node config;
  try {
    config = YAML::LoadFile(yamlPath);
  } catch (const YAML::BadFile &e) {
    LOG_ERROR("Failed to load YAML file in InitMaterial: " +
              std::string(e.what()));
    exit(EXIT_FAILURE);
  }

  auto &matManager = ctx.getMaterialManager();
  std::unordered_map<int, PDCommon::Material::Material *> idToMatMap;

  // 获取材料配置节点 (支持单数 Material 或复数 Materials)
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

        // 调用工厂模式创建实例
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

  // 将材料指针分配给各个粒子
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
// 3. 初始化物理场：根据 Solver.Type 注册核心场及材料状态变量场
// ============================================================================
void PDEngineInitializer::InitFields(PDCommon::Core::PDContext &ctx,
                                     const std::string &yamlPath) {
  LOG_INFO("Entering Field Registration Phase...");

  YAML::Node config;
  try {
    config = YAML::LoadFile(yamlPath);
  } catch (const YAML::BadFile &e) {
    LOG_ERROR("Failed to load YAML file in InitFields: " +
              std::string(e.what()));
    exit(EXIT_FAILURE);
  }

  std::string solverType = "";
  if (config["Solver"] && config["Solver"]["Type"]) {
    solverType = config["Solver"]["Type"].as<std::string>();
  }

  auto &fieldManager = ctx.getFieldManager();

  // 通过 PhysicsFieldRegistry 多态注册核心物理场 (支持组合类型如
  // "ThermalMechanical")
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

  // 注册粒子基础几何场 (Coords, Volume, PartID 等)
  PDCommon::IO::MeshData::ensureParticleFields(fieldManager);

  // 状态变量池 (SDV Pool) 初始化
  auto &matManager = ctx.getMaterialManager();
  size_t maxSDVs = 0;
  for (const auto &[matName, matPtr] : matManager.getMaterials()) {
    if (matPtr) {
      maxSDVs = std::max(maxSDVs, matPtr->getNumStateVariables());
    }
  }

  if (maxSDVs > 0) {
    fieldManager.registerField("SDV", static_cast<int>(maxSDVs));
    LOG_INFO("[InitFields] SDV Pool registered with dimension: " +
             std::to_string(maxSDVs));
  }

  // 遍历所有材料，执行材料私有状态变量场注册
  for (const auto &[matName, matPtr] : matManager.getMaterials()) {
    if (matPtr) {
      matPtr->allocateStateVariables(fieldManager);
    }
  }

  // 统一分配内存，并从 ParticleManager 填充几何数据
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
// 4. 初始化边界条件：通过 MeshReader 读取载荷并施加
// ============================================================================
void PDEngineInitializer::InitConditions(PDCommon::Core::PDContext &ctx,
                                         const std::string &yamlPath) {
  LOG_INFO("Entering Conditions Initialization Phase...");

  YAML::Node config;
  try {
    config = YAML::LoadFile(yamlPath);
  } catch (const YAML::BadFile &e) {
    LOG_ERROR("Failed to load YAML file in InitConditions: " +
              std::string(e.what()));
    exit(EXIT_FAILURE);
  }

  // 通过多态 Reader 读取网格文件（包含 *LOAD 段）
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

  // 从 meshData_.loads 中施加边界条件
  const auto &loads = reader->getMeshData().loads;
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
    // initialize 由各自的具体类型 (如 HeatFluxBC) 内部去获取自己专属的物理场并缓存换算系数
    bc->initialize(fieldManager, entry.nodeID, values);

    bcStats[entry.type]++;
    bcManager.addBC(std::move(bc));
  }

  // ==================== 【基于多态的初始状态赋值】 ====================
  // 不再硬编码检查 Temperature 或 Displacement。
  // 通过统一调用 bcManager 驱动多态下的 apply()。
  // 1. applySources: 初始化并换算由于边界带入的率场 (如 TempRate, HeatFlux, ForceDensity)
  // 2. applyConstraints: 初始化约束场状态 (如设定 t=0 时的位移边界和恒温边界)
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
// 5. 初始化邻域：读取 Horizon，构建邻居列表，生成 NeighborCount 和 BondDamage
// 键场
// ============================================================================
void PDEngineInitializer::InitNeighbors(PDCommon::Core::PDContext &ctx,
                                        const std::string &yamlPath) {
  LOG_INFO("[InitNeighbors] Entering Neighbor Search Phase...");

  YAML::Node config;
  try {
    config = YAML::LoadFile(yamlPath);
  } catch (const YAML::BadFile &e) {
    LOG_ERROR("[InitNeighbors] Failed to load YAML: " + std::string(e.what()));
    exit(EXIT_FAILURE);
  }

  // 从 YAML 读取 Solver.Horizon
  double horizon = 0.0;
  if (config["Solver"] && config["Solver"]["Horizon"]) {
    horizon = config["Solver"]["Horizon"].as<double>();
  } else {
    LOG_ERROR("[InitNeighbors] Solver.Horizon not found in YAML! Please "
              "specify the horizon radius.");
    exit(EXIT_FAILURE);
  }

  LOG_INFO("[InitNeighbors] Horizon radius: " + std::to_string(horizon));

  // 构建近邻列表（基于 Cell List 算法）
  auto &mgr = ctx.getParticleManager();
  ctx.createNeighborList(mgr, horizon);

  // 注册 NeighborCount 场并填充数据
  auto &fieldManager = ctx.getFieldManager();
  auto &neighborList = ctx.getNeighborList();
  size_t numParticles = mgr.getTotalParticles();

  fieldManager.registerField("NeighborCount", 1);
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

  // 注册 BondDamage 键场（初始值 1.0 = 完好）
  neighborList.registerBondField("BondDamage");
  double *dmgPtr = neighborList.getBondFieldPtr("BondDamage");
  size_t nBonds = neighborList.totalBonds();
  std::fill(dmgPtr, dmgPtr + nBonds, 1.0);
  LOG_INFO("[InitNeighbors] BondDamage field registered, " +
           std::to_string(nBonds) + " bonds initialized to 1.0");

  LOG_INFO("[InitNeighbors] Neighbor search phase complete.");
}

// ============================================================================
// 6. 初始化核心求解组件：从 YAML 创建 L1 (时间积分器) + L2 (PD 空间核)
// ============================================================================
void PDEngineInitializer::InitSolverComponents(
    const std::string &yamlPath,
    std::unique_ptr<Src::Integration::TimeIntegrator> &integrator,
    std::unique_ptr<PDCommon::Kernel::PDKernel> &kernel,
    Src::Integration::SolverConfig &solverConfig) {
  LOG_INFO("[PDEngine] Creating solver components (L1 + L2)...");

  YAML::Node config = YAML::LoadFile(yamlPath);

  // 初始化求解器配置参数
  if (config["Solver"]) {
    auto solverNode = config["Solver"];
    if (solverNode["TimeStep_dt"])
      solverConfig.dt = solverNode["TimeStep_dt"].as<double>();
    if (solverNode["TotalTime"])
      solverConfig.totalTime = solverNode["TotalTime"].as<double>();
    if (solverNode["OutputInterval"])
      solverConfig.outputInterval = solverNode["OutputInterval"].as<int>();
  }

  // 读取算法类型和 PD 理论类型
  std::string algorithm = "Explicit"; // 默认显式集成
  std::string pdType = "NOSB";        // 默认 NOSB 态基
  std::string physics = "Thermal";    // 默认热传导

  if (config["Solver"]) {
    auto solverNode = config["Solver"];
    if (solverNode["Algorithm"])
      algorithm = solverNode["Algorithm"].as<std::string>();
    if (solverNode["PDType"])
      pdType = solverNode["PDType"].as<std::string>();
    if (solverNode["Physics"])
      physics = solverNode["Physics"].as<std::string>();

    // 兼容旧配置：如果存在 Type 优先作为 Physics 后备
    if (!solverNode["Physics"] && solverNode["Type"])
      physics = solverNode["Type"].as<std::string>();
  }

  LOG_INFO("[PDSolver] Algorithm: " + algorithm + ", PDType: " + pdType +
           ", Physics: " + physics);

  // 创建 L1: 时间推进系统 (TimeIntegrator)
  if (algorithm == "Explicit") {
    integrator = std::make_unique<Src::Integration::ExplicitEuler>();
  } else {
    LOG_ERROR("[PDEngine] Unknown algorithm type: '" + algorithm +
              "'. Supported: Explicit");
    integrator =
        std::make_unique<Src::Integration::ExplicitEuler>(); // fallback
  }

  // 创建 L2: PD 积分核心 (PDKernel) — 反射工厂，零 if-else
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

  // 多态配置：内核自己从 YAML 中提取属于自己家族的参数
  if (config["Solver"]) {
    kernel->configure(config["Solver"]);
  }

  LOG_INFO("[PDEngine] L1: " + integrator->getName() + ", L2: " + kernelKey +
           " - components ready.");
}

} // namespace Src::Engine::Solvers::PD
