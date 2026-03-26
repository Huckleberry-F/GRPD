#include "PDEnginePre.h"
#include "Logger.h"
#include "BCManager.h"
#include "BCRegistry.h"
#include "FieldManager.h"
#include "IOManager.h"
#include "ReaderRegistry.h"
#include "MeshData.h"
#include <filesystem>
#include <map>
#include <vector>
#include <string>

namespace Src::Engine::Solvers::PD::Init {

// ============================================================================
// 步骤 4：初始化并解析边界条件
// 读取所有的载荷描述 (如 Dirichlet 位移约束、Neumann 力学边界或辐射边界类型)，
// 利用 BCRegistry 生成对应格式的条件多态实例并进行注册挂载。
// ============================================================================
void InitConditions(PDCommon::Core::PDContext &ctx,
                    const YAML::Node &config) {
  LOG_INFO("Entering Conditions Initialization Phase...");

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

    bc->initialize(fieldManager, entry.nodeID, entry.values);

    bcStats[entry.type]++;
    bcManager.addBC(std::move(bc));
  }

  bcManager.applySources();
  bcManager.applyConstraints();

  LOG_INFO("[InitConditions] Load statistics:");
  for (const auto &[type, count] : bcStats) {
      LOG_INFO("  - " + type + " : " + std::to_string(count));
  }

  LOG_INFO("[InitConditions] Boundary conditions initialization complete.");
}

} // namespace Src::Engine::Solvers::PD::Init
