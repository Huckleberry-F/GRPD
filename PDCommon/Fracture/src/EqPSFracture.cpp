#include "EqPSFracture.h"
#include "FieldManager.h"
#include "FractureRegistry.h"
#include "Logger.h"
#include "NeighborList.h"
#include "PDContext.h"
#include "ParticleManager.h"
#include <omp.h>

REGISTER_FRACTURE_MODEL(EqPSFracture, PDCommon::Fracture::EqPSFracture)

namespace PDCommon::Fracture {

void EqPSFracture::configure(const YAML::Node &node) {
  if (node["Critical_EqPS"]) {
    criticalEqPS_ = node["Critical_EqPS"].as<double>();
  } else {
    criticalEqPS_ = 0.5;
  }
  LOG_INFO("[EqPSFracture] Configured Critical_EqPS = " +
           std::to_string(criticalEqPS_));
}

void EqPSFracture::preCompute(PDCommon::Core::PDContext &ctx, int matId) {
  FractureModel::preCompute(ctx, matId);
}

void EqPSFracture::computeFracture(PDCommon::Core::PDContext &ctx, int matId) {
  auto &neighborList = ctx.getNeighborList();
  auto &manager = ctx.getParticleManager();
  auto &fieldManager = ctx.getFieldManager();

  const size_t numParticles = manager.getTotalParticles();
  // EqPlasticStrain_Trial 是由塑性本构计算后抛出的场
  auto *eqPSField = fieldManager.getFieldAs<double>("EqPlasticStrain_Trial");
  auto *activeStatusField = fieldManager.getFieldAs<int>("ActiveStatus");
  auto *bondIntegrityField = fieldManager.getFieldAs<double>("BondIntegrity");
  auto *pk1StressField = fieldManager.getFieldAs<double>("PK1Stress"); // 获取应力场进行拉压判定

  if (!eqPSField || !activeStatusField || !bondIntegrityField) {
    return;
  }

  const double *eqPSPtr = eqPSField->dataPtr();
  int *activeStatusPtr = activeStatusField->dataPtr();
  double *bondIntegrityPtr = bondIntegrityField->dataPtr();
  const double *stressPtr = pk1StressField ? pk1StressField->dataPtr() : nullptr;
  const auto &particles = manager.getAllParticles();

  // 核心循环：同时判定粒子自己的塑性状态并更新 ActiveStatus，且直接进行断键
#pragma omp parallel for schedule(guided)
  for (int i = 0; i < static_cast<int>(numParticles); ++i) {
    if (matId != -1 && particles[i].getMatId() != matId)
      continue;

    bool isFailed = (eqPSPtr[i] >= criticalEqPS_);
    
    // 强制拉伸损伤判定：仅在静水压大于 0 时才允许断裂
    if (isFailed && stressPtr) {
      int idx9 = i * 9;
      double trace = stressPtr[idx9] + stressPtr[idx9 + 4] + stressPtr[idx9 + 8];
      if (trace <= 0.0) {
        isFailed = false; // 处于压缩状态，不允许断裂发生
      }
    }

    // 2. 遍历所有邻接键，当双端均达到阈值时才强制打断并令点失活
    int activeBonds = 0;
    const int numNeighbors = neighborList.getNeighborCount(i);
    int *neighbors = neighborList.getNeighborIdsMutable(i);

    for (int k = 0; k < numNeighbors; ++k) {
      int j = neighbors[k];
      if (j == -1)
        continue;
      bool isJFailed = (eqPSPtr[j] >= criticalEqPS_);

      // 对于邻点 j 同样强制要求必须是拉伸状态才允许断裂
      if (isJFailed && stressPtr) {
        int idx9_j = j * 9;
        double trace_j = stressPtr[idx9_j] + stressPtr[idx9_j + 4] + stressPtr[idx9_j + 8];
        if (trace_j <= 0.0) {
          isJFailed = false;
        }
      }

      if (isFailed && isJFailed) {
        neighbors[k] = -1;
        activeStatusPtr[i] = 0; // 因存在键的实质性断裂，将其标记为失活点
        activeStatusPtr[j] = 0; // 因存在键的实质性断裂，将其标记为失活点
      } else {
        activeBonds++;
      }
    }
    bondIntegrityPtr[i] = calculateFractureRatio(i, activeBonds);
  }
}

} // namespace PDCommon::Fracture
