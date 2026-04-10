#include "PDEnginePre.h"
#include "Logger.h"
#include "ParticleManager.h"
#include "FieldManager.h"
#include "NeighborList.h"
#include "FieldRegistry.h"
#include "SurfaceDetector.h"

namespace Src::Engine::Solvers::PD::Init {

// ============================================================================
// 步骤 5：构建空间拓扑连接列表（近邻表）
// 根据控制球半径 (Horizon)，使用 O(N) 的网格单元搜索法建立节点关系（Bonds）。
// 完毕后注入表征损伤的基础场如 NeighborCount（初始邻居数）和 BondDamage。
// ============================================================================
void InitNeighbors(PDCommon::Core::PDContext &ctx,
                   const YAML::Node &config) {
  LOG_INFO("[InitNeighbors] ==================================================");
  LOG_INFO("[InitNeighbors] Entering Neighbor Search Phase...");
  LOG_INFO("[InitNeighbors] ==================================================");

  double horizon = 0.0;
  if (config["Solver"] && config["Solver"]["Horizon"]) {
    horizon = config["Solver"]["Horizon"].as<double>();
  } else {
    LOG_ERROR("[InitNeighbors] Solver.Horizon not found in YAML! Please "
              "specify the horizon radius.");
    exit(EXIT_FAILURE);
  }

  LOG_INFO("[InitNeighbors] Horizon radius: " + std::to_string(horizon));

  auto &mgr = ctx.getParticleManager();
  ctx.createNeighborList(mgr, horizon);

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

  // 为 FractureModel 及 Writer 提前注册全局 BondIntegrity 标量场 (防止第 0 步输出时找不到场)
   if (!fieldManager.hasField("BondIntegrity")) {
     auto bondIntegrityFieldNew = PDCommon::Field::FieldRegistry::getInstance()
         .createField("DoubleField", "BondIntegrity", 1);
     fieldManager.addField(std::move(bondIntegrityFieldNew));
   }
   auto *bondIntegrityField = fieldManager.getFieldAs<double>("BondIntegrity");
   if (bondIntegrityField) {
       bondIntegrityField->resize(numParticles);
       bondIntegrityField->clearToZero();
       LOG_INFO("[InitNeighbors] Global BondIntegrity scalar field properly initialized to 0.0");
   }

  // 计算表面粒子（Volume Deficit Method）
  PDCommon::Contact::SurfaceDetector::identifySurfaceParticles(ctx, 0.95);

  LOG_INFO("[InitNeighbors] Neighbor search phase complete.");
}

} // namespace Src::Engine::Solvers::PD::Init
