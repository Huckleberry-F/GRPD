// ============================================================================
// FractureModel.cpp - 提供所有的损伤模型通用底层逻辑
// ============================================================================

#include "FractureModel.h"
#include "PDContext.h"
#include "FieldManager.h"
#include "FieldRegistry.h"
#include "ParticleManager.h"
#include "NeighborList.h"
#include <omp.h>

namespace PDCommon::Fracture {

void FractureModel::preCompute(PDCommon::Core::PDContext &ctx, int matId) {
  auto &fieldManager = ctx.getFieldManager();
  auto &manager = ctx.getParticleManager();
  auto &neighborList = ctx.getNeighborList();
  
  const size_t numParticles = manager.getTotalParticles();

  // 1. 获取并分配全局断裂场变量
  if (!fieldManager.hasField("BondIntegrity")) {
    auto bondIntegrityField = PDCommon::Field::FieldRegistry::getInstance().createField("DoubleField", "BondIntegrity", 1);
    fieldManager.addField(std::move(bondIntegrityField));
  }
  
  auto *bondIntegrityField = fieldManager.getFieldAs<double>("BondIntegrity");
  bondIntegrityField->resize(numParticles);
  bondIntegrityField->clearToZero();
  
  // 2. 统计所有节点的初始键数量，供后续所有衍生损伤模型计算比例使用
  initialBondsCount_.assign(numParticles, 0);
  const auto &particles = manager.getAllParticles();
  
#pragma omp parallel for schedule(static)
  for (int i = 0; i < static_cast<int>(numParticles); ++i) {
    if (matId != -1 && particles[i].getMatId() != matId) continue;
    
    const int numNeighbors = neighborList.getNeighborCount(i);
    const int *neighbors = neighborList.getNeighborIds(i);
    int count = 0;
    for (int k = 0; k < numNeighbors; ++k) {
      if (neighbors[k] != -1) count++;
    }
    initialBondsCount_[i] = count;
  }
}

} // namespace PDCommon::Fracture
