// ============================================================================
// PDEngineInitializer.cpp - PD 求解引擎各组件初始化实现
// 整合了模型加载、材料分配、物理场注册、边界条件加载、邻域搜索及组件初始化
// ============================================================================

#include "PDEngineInitializer.h"
#include "GrpdReader.h"
#include "Logger.h"
#include "Material.h"
#include "Particle.h"
#include "ParticleManager.h"
#include "MaterialManager.h"
#include "FieldManager.h"
#include "NeighborList.h"
#include "ExplicitEuler.h"
#include "KernelRegistry.h"
#include "MaterialRegistry.h"
#include "PhysicsFieldRegistry.h"
#include <memory>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <yaml-cpp/yaml.h>

namespace Src::Engine::Solvers::PD {

// ============================================================================
// 1. 初始化模型：调用 Python 脚本生成网格并读取
// ============================================================================
void PDEngineInitializer::InitModel(PDCommon::Core::PDContext &ctx, const std::string &yamlPath) {
  LOG_INFO("Model pre-processing...");
  LOG_INFO("Calling Python pre-processor to generate .grpd mesh...");

  std::string pythonCommand = "python ../../Input/generate_model.py " + yamlPath;
  int pyStatus = std::system(pythonCommand.c_str());

  if (pyStatus != 0) {
    LOG_ERROR("Python pre-processor failed! Please check if python is installed and paths are correct.");
    LOG_ERROR("Exit code: " + std::to_string(pyStatus));
    exit(EXIT_FAILURE);
  }

  LOG_INFO("Mesh generated successfully by Python.");

  // 解析 YAML 获取 OutputGrpd 路径和模型名
  LOG_INFO("Parsing PD.yaml to extract model configuration...");
  std::string grpdPath = "";
  std::string currentModelName = "";

  try {
    YAML::Node config = YAML::LoadFile(yamlPath);
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

  // 设置模型上下文名称
  ctx.setName(currentModelName);
  LOG_INFO(std::string("[Context Manager] Formalized model name to: ") + ctx.getName());
  auto &manager = ctx.getParticleManager();

  // 读取 .grpd 文件并填充粒子管理器
  LOG_INFO("Reading .grpd model file: " + grpdPath + " ...");
  if (!PDCommon::IO::GrpdReader::read(grpdPath, manager)) {
    LOG_ERROR("Failed to read .grpd file!");
    exit(EXIT_FAILURE);
  }

  LOG_INFO("Total particles loaded for model " + currentModelName + ": " +
           std::to_string(manager.getTotalParticles()));

  // 验证：打印前 5 个粒子信息
  size_t previewCount = std::min(static_cast<size_t>(5), manager.getTotalParticles());
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
void PDEngineInitializer::InitMaterial(PDCommon::Core::PDContext &ctx, const std::string &yamlPath) {
  LOG_INFO("Entering Material Initialization Phase...");

  YAML::Node config;
  try {
    config = YAML::LoadFile(yamlPath);
  } catch (const YAML::BadFile &e) {
    LOG_ERROR("Failed to load YAML file in InitMaterial: " + std::string(e.what()));
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
        std::string name = matNode["Name"] ? matNode["Name"].as<std::string>() : "";

        LOG_INFO("Instantiating material [" + std::to_string(matId) + "] of type: " + type);
        std::string keyName = name.empty() ? "Mat_" + std::to_string(matId) : name;

        // 调用工厂模式创建实例
        auto matPtr = PDCommon::Material::MaterialRegistry::getInstance().createMaterial(type, keyName);

        if (matPtr) {
          matPtr->initialize(matNode);
          matPtr->setName(keyName);
          matPtr->setMatId(matId);

          idToMatMap[matId] = matPtr.get();
          matManager.addMaterial(keyName, std::move(matPtr));

          LOG_INFO("Successfully registered material ID " + std::to_string(matId) + " (" + keyName + ")");
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

  LOG_INFO("Successfully assigned materials to " + std::to_string(assignCount) + " particles.");
}

// ============================================================================
// 3. 初始化物理场：根据 Solver.Type 注册核心场及材料状态变量场
// ============================================================================
void PDEngineInitializer::InitFields(PDCommon::Core::PDContext &ctx, const std::string &yamlPath) {
  LOG_INFO("Entering Field Registration Phase...");

  YAML::Node config;
  try {
    config = YAML::LoadFile(yamlPath);
  } catch (const YAML::BadFile &e) {
    LOG_ERROR("Failed to load YAML file in InitFields: " + std::string(e.what()));
    exit(EXIT_FAILURE);
  }

  std::string solverType = "";
  if (config["Solver"] && config["Solver"]["Type"]) {
    solverType = config["Solver"]["Type"].as<std::string>();
  }

  auto &fieldManager = ctx.getFieldManager();

  // 通过 PhysicsFieldRegistry 多态注册核心物理场 (支持组合类型如 "ThermalMechanical")
  auto &registry = PDCommon::Field::PhysicsFieldRegistry::getInstance();
  bool anyMatch = false;

  for (const auto &registeredType : registry.getRegisteredTypes()) {
    if (solverType.find(registeredType) != std::string::npos) {
      auto fields = registry.create(registeredType);
      if (fields) {
        fields->registerFields(fieldManager);
        anyMatch = true;
        LOG_INFO("[InitFields] Auto-registered PhysicsFields module: " + registeredType);
      }
    }
  }

  if (!anyMatch) {
    LOG_WARNING("[InitFields] Solver type '" + solverType +
                "' did not match any registered PhysicsFields types.");
  }

  // 注册粒子基础几何场 (Coords, Volume, PartID 等)
  PDCommon::IO::GrpdReader::ensureParticleFields(fieldManager);

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
    LOG_INFO("[InitFields] SDV Pool registered with dimension: " + std::to_string(maxSDVs));
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

  if (!PDCommon::IO::GrpdReader::populateParticleFields(ctx.getParticleManager(), fieldManager)) {
    LOG_ERROR("[InitFields] Failed to populate particle fields from ParticleManager.");
    exit(EXIT_FAILURE);
  }

  LOG_INFO("[InitFields] All fields allocated for " + std::to_string(numParticles) + 
           " particles. Total fields: " + std::to_string(fieldManager.getFieldCount()) + ".");
}

// ============================================================================
// 4. 初始化边界条件：从 .grpd 文件读取并加载载荷
// ============================================================================
void PDEngineInitializer::InitConditions(PDCommon::Core::PDContext &ctx, const std::string &yamlPath) {
  LOG_INFO("Entering Conditions Initialization Phase...");

  YAML::Node config;
  try {
    config = YAML::LoadFile(yamlPath);
  } catch (const YAML::BadFile &e) {
    LOG_ERROR("Failed to load YAML file in InitConditions: " + std::string(e.what()));
    exit(EXIT_FAILURE);
  }

  // 从 .grpd 文件的 *LOAD 段读取载荷并施加到物理场
  std::string grpdPath = "";
  if (config["Assembly"] && config["Assembly"]["OutputGrpd"]) {
    grpdPath = config["Assembly"]["OutputGrpd"].as<std::string>();
  } else {
    LOG_ERROR("[InitConditions] Key [Assembly][OutputGrpd] not found! Cannot read load data.");
    return;
  }

  LOG_INFO("[InitConditions] Reading load data from .grpd file: " + grpdPath);

  if (!PDCommon::IO::GrpdReader::readLoads(grpdPath, ctx)) {
    LOG_ERROR("[InitConditions] Failed to read loads from .grpd file!");
    return;
  }

  LOG_INFO("[InitConditions] Boundary conditions initialization complete.");
}

// ============================================================================
// 5. 初始化邻域：读取 Horizon，构建邻居列表，生成 NeighborCount 和 BondDamage 键场
// ============================================================================
void PDEngineInitializer::InitNeighbors(PDCommon::Core::PDContext &ctx, const std::string &yamlPath) {
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
    LOG_ERROR("[InitNeighbors] Solver.Horizon not found in YAML! Please specify the horizon radius.");
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
      ncPtr[i] = static_cast<double>(neighborList.getNeighborCount(static_cast<int>(i)));
    }
    LOG_INFO("[InitNeighbors] NeighborCount field populated for " + std::to_string(numParticles) + " particles.");
  }

  // 注册 BondDamage 键场（初始值 1.0 = 完好）
  neighborList.registerBondField("BondDamage");
  double *dmgPtr = neighborList.getBondFieldPtr("BondDamage");
  size_t nBonds = neighborList.totalBonds();
  std::fill(dmgPtr, dmgPtr + nBonds, 1.0);
  LOG_INFO("[InitNeighbors] BondDamage field registered, " + std::to_string(nBonds) + " bonds initialized to 1.0");

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

  LOG_INFO("[PDSolver] Algorithm: " + algorithm + ", PDType: " + pdType + ", Physics: " + physics);

  // 创建 L1: 时间推进系统 (TimeIntegrator)
  if (algorithm == "Explicit") {
    integrator = std::make_unique<Src::Integration::ExplicitEuler>();
  } else {
    LOG_ERROR("[PDEngine] Unknown algorithm type: '" + algorithm + "'. Supported: Explicit");
    integrator = std::make_unique<Src::Integration::ExplicitEuler>(); // fallback
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

  LOG_INFO("[PDEngine] L1: " + integrator->getName() + ", L2: " + kernelKey + " - components ready.");
}

} // namespace Src::Engine::Solvers::PD
