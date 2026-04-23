// ============================================================================
// SurfaceDetector.cpp - 表面粒子识别工具类实现
// ============================================================================

#include "SurfaceDetector.h"
#include "FieldManager.h"
#include "FieldRegistry.h"
#include "Logger.h"
#include "NeighborList.h"
#include "PDContext.h"
#include <omp.h>
#include <vector>


namespace PDCommon::Contact {

void SurfaceDetector::identifySurfaceParticles(PDCommon::Core::PDContext &ctx,
                                               double threshold) {
  auto &fm = ctx.getFieldManager();
  auto &nl = ctx.getNeighborList();

  auto *volField = fm.getFieldAs<double>("Volume");
  if (!volField) {
    LOG_ERROR("[SurfaceDetector] 'Volume' field missing. Cannot perform Volume "
              "Deficit classification.");
    return;
  }

  size_t numParticles = volField->size();
  if (numParticles == 0) {
    return;
  }

  // 1. 若 IsSurface 场不存在则注册，否则清零准备覆盖
  if (!fm.hasField("IsSurface")) {
    auto isSurfFieldNew =
        PDCommon::Field::FieldRegistry::getInstance().createField(
            "IntField", "IsSurface", 1);
    isSurfFieldNew->resize(numParticles);
    fm.addField(std::move(isSurfFieldNew));
  }
  auto *isSurfField = fm.getFieldAs<int>("IsSurface");
  isSurfField->clearToZero();

  const double *vols = volField->dataPtr();
  int *isSurf = isSurfField->dataPtr();

  // 2. 计算每个粒子的邻居体积之和（作为完整球域的度量）
  std::vector<double> nbVolSums(numParticles, 0.0);
  double maxNbVol = 0.0;

#pragma omp parallel for
  for (int i = 0; i < static_cast<int>(numParticles); ++i) {
    double sumV = 0.0;
    int numNeighbors = nl.getNeighborCount(i);
    const int *neighbors = nl.getNeighborIds(i);
    for (int k = 0; k < numNeighbors; ++k) {
      int nb = neighbors[k];
      sumV += vols[nb];
    }
    nbVolSums[i] = sumV;
  }

  // 3. 找出全场最大的邻居体积和，以此作为内部粒子的基准标准 (100% 完整)
  for (int i = 0; i < static_cast<int>(numParticles); ++i) {
    if (nbVolSums[i] > maxNbVol) {
      maxNbVol = nbVolSums[i];
    }
  }

  // 如果最大体积和过小，说明拓扑未建或模型异常
  if (maxNbVol <= 1e-12) {
    LOG_WARNING("[SurfaceDetector] Max neighbor volume sum is zero - check "
                "horizon or mesh scale.");
    return;
  }

  // 4. 判定体积缺损严重的点为表面点
  int numSurfacePoints = 0;
#pragma omp parallel for reduction(+ : numSurfacePoints)
  for (int i = 0; i < static_cast<int>(numParticles); ++i) {
    if (nbVolSums[i] < threshold * maxNbVol) {
      isSurf[i] = 1;
      numSurfacePoints++;
    }
  }

  LOG_INFO("[SurfaceDetector] Volume Deficit check complete. Identified " +
           std::to_string(numSurfacePoints) + " surface particles out of " +
           std::to_string(numParticles) +
           " (Threshold: " + std::to_string(threshold) + ").");
}

void SurfaceDetector::updateFracturedSurfaces(PDCommon::Core::PDContext &ctx) {
  // TODO(Phase 3): Dynamic checking of freshly exposed surfaces due to
  // fracture. 预留接口：当进入损伤模拟后，可以依据附近键断裂和 ActiveStatus
  // 重新计算并增加 IsSurface。
}

} // namespace PDCommon::Contact
