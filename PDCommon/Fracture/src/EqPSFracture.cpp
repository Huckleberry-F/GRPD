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

  if (!eqPSField || !activeStatusField || !bondIntegrityField) {
    return;
  }

  const double *eqPSPtr = eqPSField->dataPtr();
  int *activeStatusPtr = activeStatusField->dataPtr();
  double *bondIntegrityPtr = bondIntegrityField->dataPtr();
  const auto &particles = manager.getAllParticles();

  // 核心循环：同时判定粒子自己的塑性状态并更新 ActiveStatus，且直接进行断键
#pragma omp parallel for schedule(guided)
  for (int i = 0; i < static_cast<int>(numParticles); ++i) {
    if (matId != -1 && particles[i].getMatId() != matId)
      continue;

    bool isFailed = (eqPSPtr[i] >= criticalEqPS_);

    // 2. 遍历所有邻接键，当双端均达到阈值时才强制打断并令点失活
    int activeBonds = 0;
    const int numNeighbors = neighborList.getNeighborCount(i);
    int *neighbors = neighborList.getNeighborIdsMutable(i);

    for (int k = 0; k < numNeighbors; ++k) {
      int j = neighbors[k];
      if (j == -1)
        continue;
      bool isJFailed = (eqPSPtr[j] >= criticalEqPS_);
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
