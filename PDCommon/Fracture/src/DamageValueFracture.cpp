#include "DamageValueFracture.h"
#include "FieldManager.h"
#include "FractureRegistry.h"
#include "Logger.h"
#include "NeighborList.h"
#include "PDContext.h"
#include "ParticleManager.h"
#include <omp.h>

REGISTER_FRACTURE_MODEL(DamageValueFracture,
                        PDCommon::Fracture::DamageValueFracture)

namespace PDCommon::Fracture {

void DamageValueFracture::configure(const YAML::Node &node) {
  if (node["Critical_Damage"]) {
    criticalDamage_ = node["Critical_Damage"].as<double>();
  } else {
    criticalDamage_ = 0.99;
  }
  LOG_INFO("[DamageValueFracture] Configured Critical_Damage = " +
           std::to_string(criticalDamage_));
}

void DamageValueFracture::preCompute(PDCommon::Core::PDContext &ctx,
                                     int matId) {
  FractureModel::preCompute(ctx, matId);
}

void DamageValueFracture::computeFracture(PDCommon::Core::PDContext &ctx,
                                          int matId) {
  auto &neighborList = ctx.getNeighborList();
  auto &manager = ctx.getParticleManager();
  auto &fieldManager = ctx.getFieldManager();

  const size_t numParticles = manager.getTotalParticles();
  auto *damageField = fieldManager.getFieldAs<double>("Damage_Trial");
  auto *activeStatusField = fieldManager.getFieldAs<int>("ActiveStatus");
  auto *bondIntegrityField = fieldManager.getFieldAs<double>("BondIntegrity");

  if (!damageField || !activeStatusField || !bondIntegrityField) {
    return;
  }

  const double *damagePtr = damageField->dataPtr();
  int *activeStatusPtr = activeStatusField->dataPtr();
  double *bondIntegrityPtr = bondIntegrityField->dataPtr();
  const auto &particles = manager.getAllParticles();

  // 核心循环：同时判定粒子自身的退化状态，并根据双端变量直接执行断键
#pragma omp parallel for schedule(guided)
  for (int i = 0; i < static_cast<int>(numParticles); ++i) {
    if (matId != -1 && particles[i].getMatId() != matId)
      continue;

    bool isFailed = (damagePtr[i] >= criticalDamage_);

    // 2. 遍历邻接键，双端均失效则打断键并让点失活
    int activeBonds = 0;
    const int numNeighbors = neighborList.getNeighborCount(i);
    int *neighbors = neighborList.getNeighborIdsMutable(i);

    for (int k = 0; k < numNeighbors; ++k) {
      int j = neighbors[k];
      if (j == -1)
        continue;
      bool isJFailed = (damagePtr[j] >= criticalDamage_);
      if (isFailed && isJFailed) {
        neighbors[k] = -1;
      } else {
        activeBonds++;
      }
    }
    
    // 更新断键比例 (0到1，1代表完全孤立)
    bondIntegrityPtr[i] = calculateFractureRatio(i, activeBonds);

    // 【阈值处决】计算本粒子的物理断键损伤度
    // 当基于周边断键的宏观损伤大于临界值时，彻底将其注销，化为无碰撞的破片
    double damage = bondIntegrityPtr[i];
    if (damage >= criticalDamage_) {
      activeStatusPtr[i] = 0; // 名正言顺地死亡，彻底化作自由破片
    }
  }
}

} // namespace PDCommon::Fracture
