// ============================================================================
// CriticalStretch.cpp
// ============================================================================

#include "CriticalStretch.h"
#include "DamageRegistry.h"
#include "PDContext.h"
#include "FieldManager.h"
#include "FieldRegistry.h"
#include "Logger.h"
#include "NeighborList.h"
#include "ParticleManager.h"
#include <cmath>
#include <omp.h>

REGISTER_DAMAGE_MODEL(CriticalStretch, PDCommon::Damage::CriticalStretch)

namespace PDCommon::Damage {

void CriticalStretch::configure(const YAML::Node &node) {
  if (node["Critical_Stretch"]) {
    criticalStretch_ = node["Critical_Stretch"].as<double>();
  } else {
    // 设置一个回退默认值，或者也可以报错
    criticalStretch_ = 0.05; 
    LOG_INFO("[CriticalStretch] 'Critical_Stretch' not found in YAML. Using default 0.05.");
  }
}

void CriticalStretch::preCompute(PDCommon::Core::PDContext &ctx, int matId) {
  auto &fieldManager = ctx.getFieldManager();
  auto &manager = ctx.getParticleManager();
  auto &neighborList = ctx.getNeighborList();
  
  const size_t numParticles = manager.getTotalParticles();

  // 1. 获取全局断裂场变量
  if (!fieldManager.hasField("Damage")) {
    auto damageField = PDCommon::Field::FieldRegistry::getInstance().createField("DoubleField", "Damage", 1);
    fieldManager.addField(std::move(damageField));
  }
  
  auto *damageField = fieldManager.getFieldAs<double>("Damage");
  damageField->resize(numParticles);
  damageField->clearToZero();
  
  // 2. 统计初始键数量，用于后面计算 Damage (1 - 完好键/初始键)
  initialBondsCount_.assign(numParticles, 0);
  const auto &particles = manager.getAllParticles();
  
#pragma omp parallel for schedule(static)
  for (int i = 0; i < static_cast<int>(numParticles); ++i) {
    if (matId != -1 && particles[i].getMatId() != matId) {
       continue;
    }
    const int numNeighbors = neighborList.getNeighborCount(i);
    const int *neighbors = neighborList.getNeighborIds(i);
    int count = 0;
    for (int k = 0; k < numNeighbors; ++k) {
      if (neighbors[k] != -1) {
        count++;
      }
    }
    initialBondsCount_[i] = count;
  }
  
  LOG_INFO("[CriticalStretch] Initialized. Critical Stretch = " + std::to_string(criticalStretch_));
}

void CriticalStretch::computeDamage(PDCommon::Core::PDContext &ctx, int matId) {
  auto &neighborList = ctx.getNeighborList();
  auto &manager = ctx.getParticleManager();
  auto &fieldManager = ctx.getFieldManager();
  
  const size_t numParticles = manager.getTotalParticles();
  auto *coordsField = fieldManager.getFieldAs<double>("Coords");
  auto *dispField = fieldManager.getFieldAs<double>("Displacement");
  auto *damageField = fieldManager.getFieldAs<double>("Damage");

  if (!coordsField || !dispField || !damageField) {
      return; 
  }

  const double *coords = coordsField->dataPtr();
  const double *disps = dispField->dataPtr();
  double *damagePtr = damageField->dataPtr();
  const auto &particles = manager.getAllParticles();

#pragma omp parallel for schedule(guided)
  for (int i = 0; i < static_cast<int>(numParticles); ++i) {
    if (matId != -1 && particles[i].getMatId() != matId) {
        continue;
    }
    int activeBonds = 0;
    const int numNeighbors = neighborList.getNeighborCount(i);
    int *neighbors = neighborList.getNeighborIdsMutable(i);
    const double* initLengths = neighborList.getBondLengths(i);
    
    double xi_x = coords[i * 3], xi_y = coords[i * 3 + 1], xi_z = coords[i * 3 + 2];
    double u_ix = disps[i * 3], u_iy = disps[i * 3 + 1], u_iz = disps[i * 3 + 2];
    
    for (int k = 0; k < numNeighbors; ++k) {
      int j = neighbors[k];
      if (j == -1) continue; // 已经断开
      
      double xj_x = coords[j * 3], xj_y = coords[j * 3 + 1], xj_z = coords[j * 3 + 2];
      double u_jx = disps[j * 3], u_jy = disps[j * 3 + 1], u_jz = disps[j * 3 + 2];
      
      double def_x = (xj_x + u_jx) - (xi_x + u_ix);
      double def_y = (xj_y + u_jy) - (xi_y + u_iy);
      double def_z = (xj_z + u_jz) - (xi_z + u_iz);
      
      double def_length = std::sqrt(def_x*def_x + def_y*def_y + def_z*def_z);
      double init_length = initLengths[k];
      
      if (init_length > 1e-16) {
        double stretch = (def_length - init_length) / init_length;
        if (stretch > criticalStretch_) {
            // 断开键！
            neighbors[k] = -1;
        } else {
            activeBonds++;
        }
      } else {
          // 初始化距离为0的奇异点
          neighbors[k] = -1;
      }
    }
    
    // 更新 Damage 指数 (0.0 表示完好，1.0 表示全断)
    int initialBonds = initialBondsCount_[i];
    if (initialBonds > 0) {
        damagePtr[i] = 1.0 - static_cast<double>(activeBonds) / static_cast<double>(initialBonds);
    } else {
        damagePtr[i] = 0.0;
    }
  }
}

} // namespace PDCommon::Damage
