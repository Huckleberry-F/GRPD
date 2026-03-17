// ============================================================================
// PD_Neighbor.cpp - PD 邻域搜索初始化
// 责任：
// 1. 从 YAML 读取 Solver.Horizon 邻域半径
// 2. 调用 NeighborList::buildNeighbors() 构建邻域列表
// 3. 注册 NeighborCount 场，记录每个粒子的邻居数
// ============================================================================

#include "PDSolver.h"
#include "FieldManager.h"
#include "Logger.h"
#include "TypedField.h"
#include <cstdlib>
#include <yaml-cpp/yaml.h>

namespace Src::Solve {

void PDSolver::InitNeighbors() {
  auto &ctx = pdContext_;
  LOG_INFO("[InitNeighbors] Entering Neighbor Search Phase...");

  // =================================================================
  // 1. 从 YAML 读取 Solver.Horizon
  // =================================================================
  YAML::Node config;
  try {
    config = YAML::LoadFile(yamlPath_);
  } catch (const YAML::BadFile &e) {
    LOG_ERROR("[InitNeighbors] Failed to load YAML: " +
              std::string(e.what()));
    exit(EXIT_FAILURE);
  }

  double horizon = 0.0;
  if (config["Solver"] && config["Solver"]["Horizon"]) {
    horizon = config["Solver"]["Horizon"].as<double>();
  } else {
    LOG_ERROR("[InitNeighbors] Solver.Horizon not found in YAML! "
              "Please specify the horizon radius.");
    exit(EXIT_FAILURE);
  }

  LOG_INFO("[InitNeighbors] Horizon radius: " + std::to_string(horizon));

  // =================================================================
  // 2. 构建近邻列表（Cell List 算法）
  // =================================================================
  auto &mgr = ctx.getParticleManager();
  ctx.createNeighborList(mgr, horizon);

  // =================================================================
  // 3. 注册 NeighborCount 场并填充数据
  // =================================================================
  auto &fieldManager = ctx.getFieldManager();
  auto &neighborList = ctx.getNeighborList();
  size_t numParticles = mgr.getTotalParticles();

  // 注册整型场 NeighborCount (维度=1)
  fieldManager.registerField("NeighborCount", 1);
  auto *ncField = fieldManager.getFieldAs<double>("NeighborCount");

  if (ncField) {
    ncField->resize(numParticles);
    double *ncPtr = ncField->dataPtr();

    for (size_t i = 0; i < numParticles; ++i) {
      ncPtr[i] = static_cast<double>(neighborList.getNeighborCount(
          static_cast<int>(i)));
    }

    LOG_INFO("[InitNeighbors] NeighborCount field populated for " +
             std::to_string(numParticles) + " particles.");
  }

  // =================================================================
  // 4. 注册 BondDamage 键场（初始值 1.0 = 完好）
  // =================================================================
  neighborList.registerBondField("BondDamage");
  double *dmgPtr = neighborList.getBondFieldPtr("BondDamage");
  size_t nBonds = neighborList.totalBonds();
  for (size_t k = 0; k < nBonds; ++k) {
    dmgPtr[k] = 1.0;
  }
  LOG_INFO("[InitNeighbors] BondDamage field registered, " +
           std::to_string(nBonds) + " bonds initialized to 1.0");

  LOG_INFO("[InitNeighbors] Neighbor search phase complete.");
}

} // namespace Src::Solve
