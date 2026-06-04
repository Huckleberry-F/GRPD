#include "BCManager.h"
#include "BCRegistry.h"
#include "IOManager.h"
#include "Logger.h"
#include "MechanicalMaterial.h"
#include "MeshData.h"
#include "PDEnginePre.h"
#include "ParticleManager.h"
#include "ReaderRegistry.h"
#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace Src::Engine::Solvers::PD::Init {

using PDCommon::BC::BC;

// ============================================================================
// 步骤 4：初始化并解析边界条件
// 读取所有的载荷描述 (如 Dirichlet 位移约束、Neumann 力学边界或辐射边界类型)，
// 利用 BCRegistry 生成对应格式的条件多态实例并进行注册挂载。
// ============================================================================
void InitConditions(PDCommon::Core::PDContext &ctx, const YAML::Node &config) {
  LOG_INFO(
      "[InitConditions] ==================================================");
  LOG_INFO("[InitConditions] Entering Conditions Initialization Phase...");
  LOG_INFO(
      "[InitConditions] ==================================================");

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

  const auto &loads = reader->getMeshData().loads;
  auto &fieldManager = ctx.getFieldManager();
  auto &bcManager = ctx.getBCManager();

  std::map<std::string, int> bcStats;

  auto &pm = ctx.getParticleManager();
  const auto &particles = pm.getAllParticles();

  double massScale = 1.0;
  if (config["Solver"] && config["Solver"]["MassScaleFactor"]) {
    massScale = config["Solver"]["MassScaleFactor"].as<double>();
  }

  double dx = 1.0;
  if (config["Parts"] && config["Parts"].IsSequence() &&
      config["Parts"].size() > 0) {
    if (config["Parts"][0]["dx"])
      dx = config["Parts"][0]["dx"].as<double>();
  }

  // 1. 按照 Step 对载荷进行分组，确保按时间顺序解析
  std::map<int, std::vector<PDCommon::IO::LoadEntry>> loadsByStep;
  for (const auto &entry : loads) {
    loadsByStep[entry.step].push_back(entry);
  }

  // 2. 状态追踪器：用于记录每个粒子上一步的最终载荷值，映射为 <节点ID,
  // 边界类型> -> 上步终值
  std::map<std::pair<int, std::string>, double> finalValuesTracker;

  // 3. 按照时间步顺序依次处理载荷
  for (auto const &[step, stepLoads] : loadsByStep) {
    for (const auto &entry : stepLoads) {
      std::string bcName = entry.type + "_BC" + std::to_string(entry.bcID) +
                           "_P" + std::to_string(entry.nodeID);

      auto bc =
          PDCommon::BC::BCRegistry::getInstance().createBC(entry.type, bcName);
      if (!bc) {
        LOG_WARNING("[InitConditions] Unknown BC type: " + entry.type);
        bcStats["Unknown"]++;
        continue;
      }

      bc->initialize(fieldManager, entry.nodeID, entry.values);
      bc->setTableNames(entry.tableNames); // 绑定表格映射名称

      // 核心功能：检查上一步是否对该节点施加了同类型载荷，若有，自动继承上一步的终点值作为本步的起点
      auto key = std::make_pair(entry.nodeID, entry.type);
      if (finalValuesTracker.count(key) > 0) {
        bc->setPrevVal(finalValuesTracker[key]);
      }

      // 更新该节点同类型载荷的最新终态目标值，供后续Step使用
      if (!entry.values.empty()) {
        finalValuesTracker[key] = entry.values[0];
      }

      double density = 1.0;
      if (entry.nodeID >= 0 && entry.nodeID < particles.size()) {
        auto *mat = dynamic_cast<PDCommon::Material::MechanicalMaterial *>(
            particles[entry.nodeID].getMaterial());
        if (mat)
          density = mat->getDensity();
      }
      // 将底层缩放因子注入给支持的面压换算高阶 BC
      bc->setScalingFactors(dx, density, massScale, ctx.getDimension(), ctx.getThickness());

      bcStats[entry.type]++;
      bcManager.addBC(std::move(bc), entry.step);
    }
  }

  // 初始施加（此时默认第一步的静态载荷，或者是通用全局载荷 Step = 0）
  BC::applySources(ctx.getBCManager(), 1.0, 0);
  BC::applyConstraints(ctx.getBCManager(), 1.0, 0);

  LOG_INFO("[InitConditions] Load statistics:");
  for (const auto &[type, count] : bcStats) {
    LOG_INFO("[InitConditions]   - " + type + " : " + std::to_string(count));
  }

  LOG_INFO("[InitConditions] Boundary conditions initialization complete.");
}

} // namespace Src::Engine::Solvers::PD::Init
