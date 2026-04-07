// ============================================================================
// EqPSDamage.cpp
// ============================================================================

#include "EqPSDamage.h"
#include "DamageRegistry.h"
#include "FieldManager.h"
#include "FieldRegistry.h"
#include "Logger.h"
#include "NeighborList.h"
#include "PDContext.h"
#include "ParticleManager.h"
#include <omp.h>

REGISTER_DAMAGE_MODEL(EqPlasticStrain, PDCommon::Damage::EqPSDamage)

namespace PDCommon::Damage {

void EqPSDamage::configure(const YAML::Node &node) {
  if (node["Critical_EqPS"]) {
    criticalEqPS_ = node["Critical_EqPS"].as<double>();
  } else {
    criticalEqPS_ = 0.15;
    LOG_INFO(
        "[EqPSDamage] 'Critical_EqPS' not found in YAML. Using default 0.15.");
  }
}

void EqPSDamage::preCompute(PDCommon::Core::PDContext &ctx, int matId) {
  // 调用基类的公共模板方法：申请 Damage 场并统计 initialBondsCount_
  DamageModel::preCompute(ctx, matId);
  LOG_INFO("[EqPSDamage] Initialized. Critical EqPS = " +
           std::to_string(criticalEqPS_));
}

void EqPSDamage::computeDamage(PDCommon::Core::PDContext &ctx, int matId) {
  auto &neighborList = ctx.getNeighborList();
  auto &manager = ctx.getParticleManager();
  auto &fieldManager = ctx.getFieldManager();

  const size_t numParticles = manager.getTotalParticles();
  auto *damageField = fieldManager.getFieldAs<double>("Damage");

  // 从塑性本构中提取等效塑性应变场 (因为在 computeDamage 时，状态大多还在
  // Trial)
  auto *eqPSField = fieldManager.getFieldAs<double>("EqPlasticStrain_Trial");

  auto *coordsField = fieldManager.getFieldAs<double>("Coords");
  auto *dispField = fieldManager.getFieldAs<double>("Displacement");

  if (!damageField || !eqPSField || !coordsField || !dispField) {
    // 弹性阶段或配置不匹配时直接返回
    return;
  }

  double *damagePtr = damageField->dataPtr();
  const double *eqPS = eqPSField->dataPtr();
  const double *coords = coordsField->dataPtr();
  const double *disp = dispField->dataPtr();
  const auto &particles = manager.getAllParticles();

#pragma omp parallel for schedule(guided)
  for (int i = 0; i < static_cast<int>(numParticles); ++i) {
    if (matId != -1 && particles[i].getMatId() != matId) {
      continue;
    }

    int activeBonds = 0;
    const int numNeighbors = neighborList.getNeighborCount(i);
    int *neighbors = neighborList.getNeighborIdsMutable(i);
    const double *bondLens = neighborList.getBondLengths(i);

    double EqPS_i = eqPS[i];

    double xi = coords[i * 3] + disp[i * 3];
    double yi = coords[i * 3 + 1] + disp[i * 3 + 1];
    double zi = coords[i * 3 + 2] + disp[i * 3 + 2];

    for (int k = 0; k < numNeighbors; ++k) {
      int j = neighbors[k];
      if (j == -1)
        continue; // 已经断开的键保留断开状态

      double EqPS_j = eqPS[j];

      double xj = coords[j * 3] + disp[j * 3];
      double yj = coords[j * 3 + 1] + disp[j * 3 + 1];
      double zj = coords[j * 3 + 2] + disp[j * 3 + 2];

      double dx = xj - xi;
      double dy = yj - yi;
      double dz = zj - zi;
      double currentLen = std::sqrt(dx * dx + dy * dy + dz * dz);
      // 用户选择的判定法:
      // 只要连线两端有任意一方完全屈服，即判定该键物理断裂，避免“死拽”现象
      if (EqPS_i > criticalEqPS_ && EqPS_j > criticalEqPS_) {
        neighbors[k] = -1; // 断开键！
      } else {
        activeBonds++;
      }
    }
    // 更新 Damage 指数，直接使用基类的内联模板方法
    damagePtr[i] = calculateDamageRatio(i, activeBonds);
  }
}

} // namespace PDCommon::Damage
