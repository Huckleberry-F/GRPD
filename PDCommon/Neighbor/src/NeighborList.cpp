// ============================================================================
// NeighborList.cpp - 两步法 Cell List + CSR 邻域搜索
// 算法流程：
// 1. 构建 Cell List（粒子→网格映射）
// 2. Pass 1（并行）：只计数，不存数据 → count_[i]
// 3. Prefix Sum（串行）：累加得到 offsets_[N+1]，分配扁平数组
// 4. Pass 2（并行）：各线程按 offsets_[i] 写入，零竞争
// ============================================================================

#include "NeighborList.h"
#include "Logger.h"
#include "ParticleManager.h"
#include <algorithm>
#include <cmath>
#include <omp.h>
#include <string>
#include <vector>

namespace PDCommon::Neighbor {

void NeighborList::buildNeighbors(
    const PDCommon::Model::ParticleManager &mgr, double horizon) {

  horizon_ = horizon;
  const int N = static_cast<int>(mgr.getTotalParticles());
  LOG_INFO("[NeighborList] Starting CSR Cell List search for " +
           std::to_string(N) +
           " particles, horizon = " + std::to_string(horizon));

  if (N == 0)
    return;

  double t0 = omp_get_wtime();

  // ===================================================================
  // 1. 获取坐标和 PartID 数据指针
  // ===================================================================
  const double *coords =
      mgr.getGeomDataPtr(PDCommon::Model::ModelVar::Coords);
  const int *partIds =
      mgr.getIntDataPtr(PDCommon::Model::ModelVar::PartID);

  // ===================================================================
  // 2. 计算包围盒
  // ===================================================================
  double xMin = coords[0], xMax = coords[0];
  double yMin = coords[1], yMax = coords[1];
  double zMin = coords[2], zMax = coords[2];

  for (int i = 1; i < N; ++i) {
    double x = coords[3 * i + 0];
    double y = coords[3 * i + 1];
    double z = coords[3 * i + 2];
    xMin = std::min(xMin, x);
    xMax = std::max(xMax, x);
    yMin = std::min(yMin, y);
    yMax = std::max(yMax, y);
    zMin = std::min(zMin, z);
    zMax = std::max(zMax, z);
  }

  const double eps = 1e-10;
  xMin -= eps;
  yMin -= eps;
  zMin -= eps;
  xMax += eps;
  yMax += eps;
  zMax += eps;

  // ===================================================================
  // 3. 构建 Cell List
  // ===================================================================
  const double cellSize = horizon;
  const int nx = std::max(1, static_cast<int>((xMax - xMin) / cellSize) + 1);
  const int ny = std::max(1, static_cast<int>((yMax - yMin) / cellSize) + 1);
  const int nz = std::max(1, static_cast<int>((zMax - zMin) / cellSize) + 1);
  const int totalCells = nx * ny * nz;

  LOG_INFO("[NeighborList] Cell grid: " + std::to_string(nx) + " x " +
           std::to_string(ny) + " x " + std::to_string(nz) + " = " +
           std::to_string(totalCells) + " cells");

  // 粒子 → cell 映射
  std::vector<std::vector<int>> cellParticles(totalCells);
  for (int i = 0; i < N; ++i) {
    int cx = std::clamp(static_cast<int>((coords[3 * i + 0] - xMin) / cellSize), 0, nx - 1);
    int cy = std::clamp(static_cast<int>((coords[3 * i + 1] - yMin) / cellSize), 0, ny - 1);
    int cz = std::clamp(static_cast<int>((coords[3 * i + 2] - zMin) / cellSize), 0, nz - 1);
    cellParticles[cx + cy * nx + cz * nx * ny].push_back(i);
  }

  double t1 = omp_get_wtime();
  LOG_INFO("[NeighborList] Cell mapping done in " +
           std::to_string((t1 - t0) * 1000.0) + " ms");

  // ===================================================================
  // 4. Pass 1：并行计数（只判断 dist2 <= h2，不存数据）
  // ===================================================================
  const double h2 = horizon * horizon;
  std::vector<int> count(N, 0);

#pragma omp parallel for schedule(dynamic, 256)
  for (int i = 0; i < N; ++i) {
    const double xi = coords[3 * i + 0];
    const double yi = coords[3 * i + 1];
    const double zi = coords[3 * i + 2];
    const int partI = partIds[i];

    int cx = std::clamp(static_cast<int>((xi - xMin) / cellSize), 0, nx - 1);
    int cy = std::clamp(static_cast<int>((yi - yMin) / cellSize), 0, ny - 1);
    int cz = std::clamp(static_cast<int>((zi - zMin) / cellSize), 0, nz - 1);

    int cnt = 0;

    for (int dz = -1; dz <= 1; ++dz) {
      int nzz = cz + dz;
      if (nzz < 0 || nzz >= nz) continue;
      for (int dy = -1; dy <= 1; ++dy) {
        int nyy = cy + dy;
        if (nyy < 0 || nyy >= ny) continue;
        for (int dx = -1; dx <= 1; ++dx) {
          int nxx = cx + dx;
          if (nxx < 0 || nxx >= nx) continue;

          for (int j : cellParticles[nxx + nyy * nx + nzz * nx * ny]) {
            if (j == i) continue;
            if (partIds[j] != partI) continue;

            double ddx = coords[3 * j + 0] - xi;
            double ddy = coords[3 * j + 1] - yi;
            double ddz = coords[3 * j + 2] - zi;

            if (ddx * ddx + ddy * ddy + ddz * ddz <= h2) {
              ++cnt;
            }
          }
        }
      }
    }
    count[i] = cnt;
  }

  double t2 = omp_get_wtime();
  LOG_INFO("[NeighborList] Pass 1 (count) done in " +
           std::to_string((t2 - t1) * 1000.0) + " ms");

  // ===================================================================
  // 5. Prefix Sum：计算 CSR 偏移量，分配扁平数组
  // ===================================================================
  offsets_.resize(N + 1);
  offsets_[0] = 0;
  for (int i = 0; i < N; ++i) {
    offsets_[i + 1] = offsets_[i] + count[i];
  }
  const size_t totalBondsCount = static_cast<size_t>(offsets_[N]);

  neighborIds_.resize(totalBondsCount);
  bondLengths_.resize(totalBondsCount);

  LOG_INFO("[NeighborList] Total bonds: " +
           std::to_string(totalBondsCount) +
           ", Memory: " +
           std::to_string((totalBondsCount * (sizeof(int) + sizeof(double))) / (1024 * 1024)) +
           " MB");

  // ===================================================================
  // 6. Pass 2：并行填充（零竞争，各线程写自己的区间）
  // ===================================================================
#pragma omp parallel for schedule(dynamic, 256)
  for (int i = 0; i < N; ++i) {
    const double xi = coords[3 * i + 0];
    const double yi = coords[3 * i + 1];
    const double zi = coords[3 * i + 2];
    const int partI = partIds[i];

    int cx = std::clamp(static_cast<int>((xi - xMin) / cellSize), 0, nx - 1);
    int cy = std::clamp(static_cast<int>((yi - yMin) / cellSize), 0, ny - 1);
    int cz = std::clamp(static_cast<int>((zi - zMin) / cellSize), 0, nz - 1);

    int writePos = offsets_[i]; // 当前粒子的写入起点

    for (int dz = -1; dz <= 1; ++dz) {
      int nzz = cz + dz;
      if (nzz < 0 || nzz >= nz) continue;
      for (int dy = -1; dy <= 1; ++dy) {
        int nyy = cy + dy;
        if (nyy < 0 || nyy >= ny) continue;
        for (int dx = -1; dx <= 1; ++dx) {
          int nxx = cx + dx;
          if (nxx < 0 || nxx >= nx) continue;

          for (int j : cellParticles[nxx + nyy * nx + nzz * nx * ny]) {
            if (j == i) continue;
            if (partIds[j] != partI) continue;

            double ddx = coords[3 * j + 0] - xi;
            double ddy = coords[3 * j + 1] - yi;
            double ddz = coords[3 * j + 2] - zi;
            double dist2 = ddx * ddx + ddy * ddy + ddz * ddz;

            if (dist2 <= h2) {
              neighborIds_[writePos] = j;
              bondLengths_[writePos] = std::sqrt(dist2);
              ++writePos;
            }
          }
        }
      }
    }
  }

  double t3 = omp_get_wtime();
  LOG_INFO("[NeighborList] Pass 2 (fill) done in " +
           std::to_string((t3 - t2) * 1000.0) + " ms");

  // ===================================================================
  // 7. 统计
  // ===================================================================
  int maxNeighbors = 0;
  for (int i = 0; i < N; ++i) {
    maxNeighbors = std::max(maxNeighbors, count[i]);
  }

  LOG_INFO("[NeighborList] Max neighbors per particle: " +
           std::to_string(maxNeighbors));
  LOG_INFO("[NeighborList] Total buildNeighbors time: " +
           std::to_string((t3 - t0) * 1000.0) + " ms");
}

void NeighborList::clear() {
  offsets_.clear();
  neighborIds_.clear();
  bondLengths_.clear();
}

} // namespace PDCommon::Neighbor
