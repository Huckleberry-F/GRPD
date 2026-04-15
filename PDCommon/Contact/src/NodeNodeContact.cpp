#include "NodeNodeContact.h"
#include "FieldManager.h"
#include "Logger.h"
#include "MechanicalMaterial.h"
#include "PDContext.h"
#include "ParticleManager.h"
#include <algorithm>
#include <cmath>
#include <omp.h>

namespace PDCommon::Contact {

// =========================================================================
// 空间哈希网格：buildCellList / computeCellHash
// =========================================================================

void NodeNodeContact::buildCellList(const double *coords, const double *disp,
                                    const int *activeStatus, double maxDx) {
  cellSize_ = maxDx * 1.05;
  minBounds_ = Eigen::Vector3d(1e10, 1e10, 1e10);
  maxBounds_ = Eigen::Vector3d(-1e10, -1e10, -1e10);

  for (int i : masterIds_) {
    if (activeStatus && activeStatus[i] == 0)
      continue;
    double cur_x = coords[i * 3] + (disp ? disp[i * 3] : 0.0);
    double cur_y = coords[i * 3 + 1] + (disp ? disp[i * 3 + 1] : 0.0);
    double cur_z = coords[i * 3 + 2] + (disp ? disp[i * 3 + 2] : 0.0);
    minBounds_.x() = std::min(minBounds_.x(), cur_x);
    minBounds_.y() = std::min(minBounds_.y(), cur_y);
    minBounds_.z() = std::min(minBounds_.z(), cur_z);
    maxBounds_.x() = std::max(maxBounds_.x(), cur_x);
    maxBounds_.y() = std::max(maxBounds_.y(), cur_y);
    maxBounds_.z() = std::max(maxBounds_.z(), cur_z);
  }

  Eigen::Vector3d range = maxBounds_ - minBounds_;
  if (range.x() < cellSize_) {
    minBounds_.x() -= cellSize_;
    maxBounds_.x() += cellSize_;
  }
  if (range.y() < cellSize_) {
    minBounds_.y() -= cellSize_;
    maxBounds_.y() += cellSize_;
  }
  if (range.z() < cellSize_) {
    minBounds_.z() -= cellSize_;
    maxBounds_.z() += cellSize_;
  }

  gridDims_.x() = static_cast<int>(
      std::ceil((maxBounds_.x() - minBounds_.x()) / cellSize_));
  gridDims_.y() = static_cast<int>(
      std::ceil((maxBounds_.y() - minBounds_.y()) / cellSize_));
  gridDims_.z() = static_cast<int>(
      std::ceil((maxBounds_.z() - minBounds_.z()) / cellSize_));

  size_t numCells =
      static_cast<size_t>(gridDims_.x()) * gridDims_.y() * gridDims_.z();
  head_.assign(numCells, -1);

  for (int i : masterIds_) {
    if (activeStatus && activeStatus[i] == 0)
      continue;
    double cur_x = coords[i * 3] + (disp ? disp[i * 3] : 0.0);
    double cur_y = coords[i * 3 + 1] + (disp ? disp[i * 3 + 1] : 0.0);
    double cur_z = coords[i * 3 + 2] + (disp ? disp[i * 3 + 2] : 0.0);
    int cellHash = computeCellHash(cur_x, cur_y, cur_z);
    next_[i] = head_[cellHash];
    head_[cellHash] = static_cast<int>(i);
  }
}

int NodeNodeContact::computeCellHash(double x, double y, double z) const {
  int cx = static_cast<int>(std::floor((x - minBounds_.x()) / cellSize_));
  int cy = static_cast<int>(std::floor((y - minBounds_.y()) / cellSize_));
  int cz = static_cast<int>(std::floor((z - minBounds_.z()) / cellSize_));
  cx = std::max(0, std::min(cx, gridDims_.x() - 1));
  cy = std::max(0, std::min(cy, gridDims_.y() - 1));
  cz = std::max(0, std::min(cz, gridDims_.z() - 1));
  return cx + cy * gridDims_.x() + cz * gridDims_.x() * gridDims_.y();
}

// =========================================================================
// 模板方法骨架：computeContactForce
// =========================================================================

void NodeNodeContact::computeContactForce(PDCommon::Core::PDContext &ctx) {
  auto &fm = ctx.getFieldManager();
  auto &pm = ctx.getParticleManager();
  size_t numParticles = pm.getTotalParticles();

  // --- 1. 提取公共场数据 ---
  auto *activeField = fm.getFieldAs<int>("ActiveStatus");
  auto *coordsField = fm.getFieldAs<double>("Coords");
  auto *dispField = fm.getFieldAs<double>("Displacement");
  auto *volField = fm.getFieldAs<double>("Volume");
  auto *accField = fm.getFieldAs<double>("Acceleration");
  auto *velField = fm.getFieldAs<double>("Velocity");
  auto *partField = fm.getFieldAs<int>("PartID");

  if (!coordsField || !volField || !accField)
    return;

  const int *activeStatusPtr = activeField ? activeField->dataPtr() : nullptr;
  const double *coords = coordsField->dataPtr();
  const double *disp = dispField ? dispField->dataPtr() : nullptr;
  const double *vols = volField->dataPtr();
  double *acc = accField->dataPtr();
  const double *vel = velField ? velField->dataPtr() : nullptr;
  const int *partIDs = partField ? partField->dataPtr() : nullptr;
  const auto &neighborList = ctx.getNeighborList();
  const auto &particles = pm.getAllParticles();

  // --- 2. 计算 maxDx ---
  double maxVol = 0.0;
  for (int i : masterIds_) {
    if (vols[i] > maxVol)
      maxVol = vols[i];
  }
  if (maxVol <= 0.0)
    return;
  // 2D/3D 维度感知的 dx 推导
  const int dim = ctx.getDimension();
  const double thickness = ctx.getThickness();
  auto volToDx = [dim, thickness](double v) -> double {
    return (dim == 2) ? std::sqrt(v / thickness) : std::cbrt(v);
  };
  double maxDx = volToDx(maxVol);

  // --- 3. 子类预处理钩子（如自动估算罚函数刚度等） ---
  onPreContact(ctx, maxDx);

  // --- 4. 分配链表 + 构建空间网格 ---
  if (next_.size() < numParticles)
    next_.resize(numParticles, -1);
  buildCellList(coords, disp, activeStatusPtr, maxDx);

  // --- 5. OMP 并行遍历 slave 粒子 ---
#pragma omp parallel for schedule(dynamic)
  for (size_t idx = 0; idx < slaveIds_.size(); ++idx) {
    int i = slaveIds_[idx];
    if (activeStatusPtr && activeStatusPtr[i] == 0)
      continue;

    double xi = coords[i * 3] + (disp ? disp[i * 3] : 0.0);
    double yi = coords[i * 3 + 1] + (disp ? disp[i * 3 + 1] : 0.0);
    double zi = coords[i * 3 + 2] + (disp ? disp[i * 3 + 2] : 0.0);
    double dx_i = volToDx(vols[i]);

    auto *mat = dynamic_cast<PDCommon::Material::MechanicalMaterial *>(
        particles[i].getMaterial());
    double rho = mat ? mat->getDensity() : 1.0;
    // 【物理修正】：同时计入质量放大系数，保持接触物理与主方程时间尺度绝对对齐
    double mass_i = rho * vols[i] * massScaleFactor_;

    int cx = static_cast<int>(std::floor((xi - minBounds_.x()) / cellSize_));
    int cy = static_cast<int>(std::floor((yi - minBounds_.y()) / cellSize_));
    int cz = static_cast<int>(std::floor((zi - minBounds_.z()) / cellSize_));

    double fx = 0.0, fy = 0.0, fz = 0.0;

    // --- 27 邻格遍历 ---
    for (int offset_x = -1; offset_x <= 1; ++offset_x) {
      for (int offset_y = -1; offset_y <= 1; ++offset_y) {
        for (int offset_z = -1; offset_z <= 1; ++offset_z) {
          int ncx = cx + offset_x, ncy = cy + offset_y, ncz = cz + offset_z;
          if (ncx < 0 || ncx >= gridDims_.x())
            continue;
          if (ncy < 0 || ncy >= gridDims_.y())
            continue;
          if (ncz < 0 || ncz >= gridDims_.z())
            continue;

          int hash =
              ncx + ncy * gridDims_.x() + ncz * gridDims_.x() * gridDims_.y();
          int j = head_[hash];

          while (j != -1) {
            if (i != j) {
              // master 侧碎片过滤
              if (activeStatusPtr && activeStatusPtr[j] == 0) {
                j = next_[j];
                continue;
              }

              double xj = coords[j * 3] + (disp ? disp[j * 3] : 0.0);
              double yj = coords[j * 3 + 1] + (disp ? disp[j * 3 + 1] : 0.0);
              double zj = coords[j * 3 + 2] + (disp ? disp[j * 3 + 2] : 0.0);

              double rx = xi - xj, ry = yi - yj, rz = zi - zj;
              double distSqr = rx * rx + ry * ry + rz * rz;

              if (distSqr > 1e-14) {
                // 初始邻居过滤（同一 Part 的 PD 邻域键不参与接触）
                bool isInitialNeighbor = false;
                if (partIDs && partIDs[i] == partIDs[j]) {
                  const int *nbIds = neighborList.getNeighborIds(i);
                  int numNb = neighborList.getNeighborCount(i);
                  for (int n_idx = 0; n_idx < numNb; ++n_idx) {
                    if (nbIds[n_idx] == j) {
                      isInitialNeighbor = true;
                      break;
                    }
                  }
                }
                if (isInitialNeighbor) {
                  j = next_[j];
                  continue;
                }

                double dist = std::sqrt(distSqr);
                double dx_j = volToDx(vols[j]);
                double safeDist = (dx_i + dx_j) / 2.0;

                if (dist < safeDist) {
                  double raw_penetration = safeDist - dist;
                  // 【穿透截断保护】：限制最大穿透量不超过安全距离的50%
                  // 当粒子几乎重合(dist→0)时，防止：
                  //   1. 力无限增大导致数值爆炸
                  //   2. 法线方向因 rx/dist 中 dist→0 而不稳定
                  //   3. 粒子穿过对方后法线翻转，力变成加速穿透的"吸引力"
                  raw_penetration = std::min(raw_penetration, safeDist * 0.5);
                  double nx = rx / dist, ny = ry / dist, nz = rz / dist;

                  // 计算 master 质量
                  auto *matJ =
                      dynamic_cast<PDCommon::Material::MechanicalMaterial *>(
                          particles[j].getMaterial());
                  double rhoJ = matJ ? matJ->getDensity() : 1.0;
                  double massJ = rhoJ * vols[j] * massScaleFactor_;

                  // 构造碰撞对上下文
                  ContactPairContext pair;
                  pair.i = i;
                  pair.j = j;
                  pair.dist = dist;
                  pair.safeDist = safeDist;
                  pair.raw_penetration = raw_penetration;
                  pair.nx = nx;
                  pair.ny = ny;
                  pair.nz = nz;
                  pair.mass_i = mass_i;
                  pair.mass_j = massJ;
                  pair.dx_i = dx_i;
                  pair.dx_j = dx_j;
                  pair.vel = vel;

                  // 调用子类虚函数计算碰撞力
                  ContactPairResult result = computePairForce(pair);

                  if (result.valid) {
                    // 累加 slave 侧力
                    fx += result.fx;
                    fy += result.fy;
                    fz += result.fz;

                    // 反作用力注入 master（牛顿第三定律）
                    if (massJ > 1e-30) {
#pragma omp atomic
                      acc[j * 3] -= result.fx / massJ;
#pragma omp atomic
                      acc[j * 3 + 1] -= result.fy / massJ;
#pragma omp atomic
                      acc[j * 3 + 2] -= result.fz / massJ;
                    }
                  }
                }
              }
            }
            j = next_[j];
          }
        }
      }
    }

    // 注入 slave 加速度
    if (mass_i > 1e-30) {
      acc[i * 3] += fx / mass_i;
      acc[i * 3 + 1] += fy / mass_i;
      acc[i * 3 + 2] += fz / mass_i;
    }
  }
}

} // namespace PDCommon::Contact
