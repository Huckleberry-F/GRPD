// ============================================================================
// NeighborList.cpp - 全扁平化 Cell List + CSR 邻域搜索
// 特点：
// 1. Cell List 使用计数排序 (Counting Sort)，零嵌套 vector
// 2. CSR 两步法：Pass1 计数 → Prefix Sum → Pass2 填充
// 3. 全程只有连续数组访问，100% 缓存友好
// ============================================================================

#include "NeighborList.h"
#include "Logger.h"
#include "ParticleManager.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <omp.h>
#include <string>
#include <vector>

namespace PDCommon::Neighbor {

// 辅助函数：计算粒子所在的 cell 索引
static inline int computeCellIndex(double x, double y, double z, double xMin,
                                   double yMin, double zMin, double cellSize,
                                   int nx, int ny, int nz) {
  int cx = std::clamp(static_cast<int>((x - xMin) / cellSize), 0, nx - 1);
  int cy = std::clamp(static_cast<int>((y - yMin) / cellSize), 0, ny - 1);
  int cz = std::clamp(static_cast<int>((z - zMin) / cellSize), 0, nz - 1);
  return cx + cy * nx + cz * nx * ny;
}

void NeighborList::buildNeighbors(const PDCommon::Model::ParticleManager &mgr,
                                  double horizon) {
  horizon_ = horizon;
  const int N = static_cast<int>(mgr.getTotalParticles());
  LOG_INFO("[NeighborList] Starting full-flat CSR search for " +
           std::to_string(N) +
           " particles, horizon = " + std::to_string(horizon));

  if (N == 0)
    return;

  double t0 = omp_get_wtime();

  // ===================================================================
  // 1. 获取数据指针
  // ===================================================================
  const double *coords = mgr.getGeomDataPtr(PDCommon::Model::ModelVar::Coords);
  const int *partIds = mgr.getIntDataPtr(PDCommon::Model::ModelVar::PartID);

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
  // 3. 网格尺寸
  // ===================================================================
  const double cellSize = horizon;
  const int nx = std::max(1, static_cast<int>((xMax - xMin) / cellSize) + 1);
  const int ny = std::max(1, static_cast<int>((yMax - yMin) / cellSize) + 1);
  const int nz = std::max(1, static_cast<int>((zMax - zMin) / cellSize) + 1);
  const int totalCells = nx * ny * nz;

  LOG_INFO("[NeighborList] Cell grid: " + std::to_string(nx) + " x " +
           std::to_string(ny) + " x " + std::to_string(nz) + " = " +
           std::to_string(totalCells) + " cells");

  // ===================================================================
  // 4. 扁平化 Cell List（计数排序，零嵌套 vector）
  //    cellOffset[c] ~ cellOffset[c+1] 是 cell c 中的粒子在 cellData 中的范围
  //    cellData[k] = 粒子 ID
  // ===================================================================

  // 4a. 计数每个 cell 中有多少粒子
  std::vector<int> cellCount(totalCells, 0);
  std::vector<int> particleCellIdx(N); // 每个粒子的 cell 索引

  for (int i = 0; i < N; ++i) {
    int c =
        computeCellIndex(coords[3 * i], coords[3 * i + 1], coords[3 * i + 2],
                         xMin, yMin, zMin, cellSize, nx, ny, nz);
    particleCellIdx[i] = c;
    cellCount[c]++;
  }

  // 4b. Prefix Sum → cell 偏移量
  std::vector<int> cellOffset(totalCells + 1);
  cellOffset[0] = 0;
  for (int c = 0; c < totalCells; ++c) {
    cellOffset[c + 1] = cellOffset[c] + cellCount[c];
  }

  // 4c. 填充扁平 cellData
  std::vector<int> cellData(N);
  std::vector<int> cellWritePos(totalCells, 0); // 每个 cell 的写入游标

  for (int i = 0; i < N; ++i) {
    int c = particleCellIdx[i];
    int pos = cellOffset[c] + cellWritePos[c];
    cellData[pos] = i;
    cellWritePos[c]++;
  }

  double t1 = omp_get_wtime();
  LOG_INFO("[NeighborList] Flat cell list built in " +
           std::to_string((t1 - t0) * 1000.0) + " ms");

  // ===================================================================
  // 5. Pass 1：并行计数邻居（纯连续内存遍历）
  // ===================================================================
  const double h2 = horizon * horizon;
  std::vector<int> count(N, 0);

#pragma omp parallel for schedule(static)
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
      if (nzz < 0 || nzz >= nz)
        continue;
      for (int dy = -1; dy <= 1; ++dy) {
        int nyy = cy + dy;
        if (nyy < 0 || nyy >= ny)
          continue;
        for (int dx = -1; dx <= 1; ++dx) {
          int nxx = cx + dx;
          if (nxx < 0 || nxx >= nx)
            continue;

          int neighborCell = nxx + nyy * nx + nzz * nx * ny;
          // 遍历该 cell 中的粒子（连续内存访问！）
          for (int k = cellOffset[neighborCell];
               k < cellOffset[neighborCell + 1]; ++k) {
            int j = cellData[k];
            if (j == i)
              continue;
            if (partIds[j] != partI)
              continue;

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
  // 6. Prefix Sum → CSR 偏移量 + 分配扁平数组
  // ===================================================================
  offsets_.resize(N + 1);
  offsets_[0] = 0;
  for (int i = 0; i < N; ++i) {
    offsets_[i + 1] = offsets_[i] + count[i];
  }
  const size_t totalBondsCount = static_cast<size_t>(offsets_[N]);

  neighborIds_.resize(totalBondsCount);
  bondLengths_.resize(totalBondsCount);

  LOG_INFO("[NeighborList] Total bonds: " + std::to_string(totalBondsCount) +
           ", Memory: " +
           std::to_string((totalBondsCount * (sizeof(int) + sizeof(double))) /
                          (1024 * 1024)) +
           " MB");

  // ===================================================================
  // 7. Pass 2：并行填充（零竞争，连续内存）
  // ===================================================================
#pragma omp parallel for schedule(static)
  for (int i = 0; i < N; ++i) {
    const double xi = coords[3 * i + 0];
    const double yi = coords[3 * i + 1];
    const double zi = coords[3 * i + 2];
    const int partI = partIds[i];

    int cx = std::clamp(static_cast<int>((xi - xMin) / cellSize), 0, nx - 1);
    int cy = std::clamp(static_cast<int>((yi - yMin) / cellSize), 0, ny - 1);
    int cz = std::clamp(static_cast<int>((zi - zMin) / cellSize), 0, nz - 1);

    int writePos = offsets_[i];

    for (int dz = -1; dz <= 1; ++dz) {
      int nzz = cz + dz;
      if (nzz < 0 || nzz >= nz)
        continue;
      for (int dy = -1; dy <= 1; ++dy) {
        int nyy = cy + dy;
        if (nyy < 0 || nyy >= ny)
          continue;
        for (int dx = -1; dx <= 1; ++dx) {
          int nxx = cx + dx;
          if (nxx < 0 || nxx >= nx)
            continue;

          int neighborCell = nxx + nyy * nx + nzz * nx * ny;
          for (int k = cellOffset[neighborCell];
               k < cellOffset[neighborCell + 1]; ++k) {
            int j = cellData[k];
            if (j == i)
              continue;
            if (partIds[j] != partI)
              continue;

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
  // 8. 统计
  // ===================================================================
  int maxNeighbors = 0;
  for (int i = 0; i < N; ++i) {
    maxNeighbors = std::max(maxNeighbors, count[i]);
  }

  LOG_INFO("[NeighborList] Max neighbors: " + std::to_string(maxNeighbors));
  LOG_INFO("[NeighborList] Total time: " + std::to_string((t3 - t0) * 1000.0) +
           " ms");
}

void NeighborList::clear() {
  offsets_.clear();
  neighborIds_.clear();
  bondLengths_.clear();
  bondFields_.clear();
}

// =====================================================================
// BondField 动态注册实现
// =====================================================================

void NeighborList::registerBondField(const std::string &name) {
  if (bondFields_.count(name)) {
    return; // 已注册，跳过
  }
  // 分配 totalBonds 大小的连续数组，初始化为 0
  bondFields_[name].assign(neighborIds_.size(), 0.0);
}

double *NeighborList::getBondFieldPtr(const std::string &name) {
  auto it = bondFields_.find(name);
  if (it == bondFields_.end()) return nullptr;
  return it->second.data();
}

const double *NeighborList::getBondFieldPtr(const std::string &name) const {
  auto it = bondFields_.find(name);
  if (it == bondFields_.end()) return nullptr;
  return it->second.data();
}

bool NeighborList::hasBondField(const std::string &name) const {
  return bondFields_.count(name) > 0;
}

double *NeighborList::getBondFieldForParticle(const std::string &name, int i) {
  auto it = bondFields_.find(name);
  if (it == bondFields_.end()) return nullptr;
  return &(it->second[offsets_[i]]);
}

const double *NeighborList::getBondFieldForParticle(const std::string &name,
                                                     int i) const {
  auto it = bondFields_.find(name);
  if (it == bondFields_.end()) return nullptr;
  return &(it->second[offsets_[i]]);
}

} // namespace PDCommon::Neighbor
