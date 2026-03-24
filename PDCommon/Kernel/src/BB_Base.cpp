#include "BB_Base.h"
#include "FieldManager.h"
#include "NeighborList.h"
#include "ParticleManager.h"
#include "Logger.h"
#include <vector>
#include <omp.h>

namespace PDCommon::Kernel {

void BB_Base::configure(const YAML::Node &solverNode) {
  if (solverNode["KernelWeightType"]) {
    std::string kt = solverNode["KernelWeightType"].as<std::string>();
    if (kt == "Constant") kernelType_ = InfluenceKernelType::Constant;
    else if (kt == "Linear") kernelType_ = InfluenceKernelType::Linear;
    // ...
    LOG_INFO("[BB_Base] Applied KernelWeightType: " + kt);
  }
}

void BB_Base::ComputeSurfaceCorrections(PDCommon::Core::PDContext &ctx) {
  auto &manager = ctx.getParticleManager();
  auto &neighborList = ctx.getNeighborList();
  auto &fieldManager = ctx.getFieldManager();

  const size_t numParticles = manager.getTotalParticles();
  const double horizon = neighborList.getHorizon();
  const int dim = ctx.getDimension();

  auto *volumeField = fieldManager.getFieldAs<double>("Volume");
  if (!volumeField) return;
  const double *volumes = volumeField->dataPtr();

  LOG_INFO("[BB_Base] Computing Volume Surface Correction Factors...");

  // 1. 理论完整体积
  const double PI = 3.14159265358979323846;
  double dx_ref = std::cbrt(volumes[0]);
  double V_complete;
  if (dim == 2) {
    V_complete = PI * horizon * horizon * dx_ref; 
  } else {
    V_complete = (4.0 / 3.0) * PI * horizon * horizon * horizon;
  }

  // 2. 计算每个粒子的实际邻域体积
  std::vector<double> volActual(numParticles, 0.0);
  #pragma omp parallel for schedule(static)
  for (int i = 0; i < static_cast<int>(numParticles); ++i) {
    double vSum = 0.0;
    const int numNeighbors = neighborList.getNeighborCount(i);
    const int *neighbors = neighborList.getNeighborIds(i);
    for (int k = 0; k < numNeighbors; ++k) {
      if (neighbors[k] != -1) vSum += volumes[neighbors[k]];
    }
    volActual[i] = vSum;
  }

  // 3. 将 g_ij 写入 BondField
  neighborList.registerBondField("SurfaceCorrection");
  double *gPtr = neighborList.getBondFieldPtr("SurfaceCorrection");

  #pragma omp parallel for schedule(static)
  for (int i = 0; i < static_cast<int>(numParticles); ++i) {
    double g_i = (volActual[i] > 1e-30) ? (V_complete / volActual[i]) : 1.0;
    if (g_i > 5.0) g_i = 5.0; // 限幅

    const int numNeighbors = neighborList.getNeighborCount(i);
    const int *neighbors = neighborList.getNeighborIds(i);
    const int offset = neighbors - neighborList.getNeighborIds(0);

    for (int k = 0; k < numNeighbors; ++k) {
      int j = neighbors[k];
      if (j == -1) {
        gPtr[offset + k] = 1.0;
        continue;
      }

      double g_j = (volActual[j] > 1e-30) ? (V_complete / volActual[j]) : 1.0;
      if (g_j > 5.0) g_j = 5.0;
      gPtr[offset + k] = std::sqrt(g_i * g_j); // 几何平均
    }
  }

  LOG_INFO("[BB_Base] Surface Correction applied.");
}

} // namespace PDCommon::Kernel
