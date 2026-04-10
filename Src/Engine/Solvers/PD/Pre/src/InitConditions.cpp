#include "BCManager.h"
#include "BCRegistry.h"
#include "FieldManager.h"
#include "IOManager.h"
#include "Logger.h"
#include "MeshData.h"
#include "PDEnginePre.h"
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
    bc->setTableNames(entry.tableNames); // 绑定表格映射名称

    bcStats[entry.type]++;
    bcManager.addBC(std::move(bc), entry.step);
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
