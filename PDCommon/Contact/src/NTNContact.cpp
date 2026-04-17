#include "NTNContact.h"
#include "ContactRegistry.h"
#include "FieldManager.h"
#include "Logger.h"
#include "MechanicalMaterial.h"
#include "PDContext.h"
#include "ParticleManager.h"
#include <algorithm>
#include <cmath>
#include <omp.h>

namespace PDCommon::Contact {

NTNContact::NTNContact(const std::string &name,
                       std::unique_ptr<IContactForceLaw> forceLaw)
    : IContactAlgorithm(name), forceLaw_(std::move(forceLaw)) {}

void NTNContact::initialize(const YAML::Node &configNode) {
  if (forceLaw_) {
    forceLaw_->initialize(configNode);
  }
}

// =========================================================================
// 空间哈希网格探测内置逻辑
// =========================================================================

void NTNContact::buildCellList(const double *coords, const double *disp,
                               double maxDx) {
  cellSize_ = maxDx * 1.05;
  minBounds_ = Eigen::Vector3d(1e10, 1e10, 1e10);
  maxBounds_ = Eigen::Vector3d(-1e10, -1e10, -1e10);

  for (int i : masterIds_) {
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
    double cur_x = coords[i * 3] + (disp ? disp[i * 3] : 0.0);
    double cur_y = coords[i * 3 + 1] + (disp ? disp[i * 3 + 1] : 0.0);
    double cur_z = coords[i * 3 + 2] + (disp ? disp[i * 3 + 2] : 0.0);
    int cellHash = computeCellHash(cur_x, cur_y, cur_z);
    next_[i] = head_[cellHash];
    head_[cellHash] = static_cast<int>(i);
  }
}

int NTNContact::computeCellHash(double x, double y, double z) const {
  int cx = static_cast<int>(std::floor((x - minBounds_.x()) / cellSize_));
  int cy = static_cast<int>(std::floor((y - minBounds_.y()) / cellSize_));
  int cz = static_cast<int>(std::floor((z - minBounds_.z()) / cellSize_));
  cx = std::max(0, std::min(cx, gridDims_.x() - 1));
  cy = std::max(0, std::min(cy, gridDims_.y() - 1));
  cz = std::max(0, std::min(cz, gridDims_.z() - 1));
  return cx + cy * gridDims_.x() + cz * gridDims_.x() * gridDims_.y();
}

// =========================================================================
// 核心接触力计算（合二为一）
// =========================================================================

void NTNContact::computeContactForce(PDCommon::Core::PDContext &ctx) {
  if (!forceLaw_) {
    LOG_ERROR("[NTNContact] ForceLaw is null!");
    return;
  }

  // 同步粒子 ID 给 ForceLaw（如果其需要预判刚度）
  forceLaw_->setContactParticleIds(masterIds_, slaveIds_);

  auto &fm = ctx.getFieldManager();
  auto &pm = ctx.getParticleManager();
  size_t numParticles = pm.getTotalParticles();

  // 1. 提取公共场数据
  auto *activeField = fm.getFieldAs<int>("ActiveStatus");
  auto *coordsField = fm.getFieldAs<double>("Coords");
  auto *dispField = fm.getFieldAs<double>("Displacement");
  auto *volField = fm.getFieldAs<double>("Volume");
  auto *velField = fm.getFieldAs<double>("Velocity");
  auto *accField = fm.getFieldAs<double>("Acceleration");
  auto *partField = fm.getFieldAs<int>("PartID");

  if (!coordsField || !volField || !accField)
    return;

  const int *activeStatusPtr = activeField ? activeField->dataPtr() : nullptr;
  const double *coords = coordsField->dataPtr();
  const double *disp = dispField ? dispField->dataPtr() : nullptr;
  const double *vols = volField->dataPtr();
  const double *vel = velField ? velField->dataPtr() : nullptr;
  double *acc = accField->dataPtr();
  const int *partIDs = partField ? partField->dataPtr() : nullptr;
  const auto &neighborList = ctx.getNeighborList();
  const auto &particles = pm.getAllParticles();

  // 2. 计算 maxDx
  double maxVol = 0.0;
  for (int i : masterIds_) {
    if (vols[i] > maxVol)
      maxVol = vols[i];
  }
  if (maxVol <= 0.0)
    return;

  const int dim = ctx.getDimension();
  const double thickness = ctx.getThickness();
  auto volToDx = [dim, thickness](double v) -> double {
    return (dim == 2) ? std::sqrt(v / thickness) : std::cbrt(v);
  };
  double maxDx = volToDx(maxVol);
  lastMaxDx_ = maxDx;

  forceLaw_->onPreContact(ctx, maxDx);

  // 3. 分配链表 + 构建空间网格
  if (next_.size() < numParticles)
    next_.resize(numParticles, -1);
  buildCellList(coords, disp, maxDx);

  // 4. OMP 并行遍历 slave 粒子，一次性完成搜索与加力施加
#pragma omp parallel for schedule(dynamic)
  for (size_t idx = 0; idx < slaveIds_.size(); ++idx) {
    int i = slaveIds_[idx];

    double xi = coords[i * 3] + (disp ? disp[i * 3] : 0.0);
    double yi = coords[i * 3 + 1] + (disp ? disp[i * 3 + 1] : 0.0);
    double zi = coords[i * 3 + 2] + (disp ? disp[i * 3 + 2] : 0.0);
    double dx_i = volToDx(vols[i]);

    auto *mat = dynamic_cast<PDCommon::Material::MechanicalMaterial *>(
        particles[i].getMaterial());
    double rho = mat ? mat->getDensity() : 1.0;
    double mass_i = rho * vols[i] * massScaleFactor_;

    int cx = static_cast<int>(std::floor((xi - minBounds_.x()) / cellSize_));
    int cy = static_cast<int>(std::floor((yi - minBounds_.y()) / cellSize_));
    int cz = static_cast<int>(std::floor((zi - minBounds_.z()) / cellSize_));

    // 27 邻格遍历查找可能的 master 粒子 j
    for (int offset_x = -1; offset_x <= 1; ++offset_x) {
      for (int offset_y = -1; offset_y <= 1; ++offset_y) {
        for (int offset_z = -1; offset_z <= 1; ++offset_z) {
          int ncx = cx + offset_x, ncy = cy + offset_y, ncz = cz + offset_z;
          if (ncx < 0 || ncx >= gridDims_.x() || ncy < 0 ||
              ncy >= gridDims_.y() || ncz < 0 || ncz >= gridDims_.z())
            continue;

          int hash =
              ncx + ncy * gridDims_.x() + ncz * gridDims_.x() * gridDims_.y();
          int j = head_[hash];

          while (j != -1) {
            if (i != j) {
              double xj = coords[j * 3] + (disp ? disp[j * 3] : 0.0);
              double yj = coords[j * 3 + 1] + (disp ? disp[j * 3 + 1] : 0.0);
              double zj = coords[j * 3 + 2] + (disp ? disp[j * 3 + 2] : 0.0);

              double rx = xi - xj, ry = yi - yj, rz = zi - zj;
              double distSqr = rx * rx + ry * ry + rz * rz;

              if (distSqr > 1e-14) {
                // 初始邻居过滤
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

                if (!isInitialNeighbor) {
                  double dist = std::sqrt(distSqr);
                  double dx_j = volToDx(vols[j]);
                  double safeDist = (dx_i + dx_j) / 2.0;

                  if (dist < safeDist) {
                    double raw_penetration = safeDist - dist;
                    raw_penetration = std::min(raw_penetration, safeDist * 0.5);
                    double nx = rx / dist, ny = ry / dist, nz = rz / dist;

                    auto *matJ =
                        dynamic_cast<PDCommon::Material::MechanicalMaterial *>(
                            particles[j].getMaterial());
                    double rhoJ = matJ ? matJ->getDensity() : 1.0;
                    double massJ = rhoJ * vols[j] * massScaleFactor_;

                    // 构造碰撞对上下文交给 ForceLaw
                    ContactContext pair;
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

                    // 获取接触力 (从 ForceLaw)
                    ForceResult forceRes = forceLaw_->computeForce(pair);

                    if (forceRes.valid) {
                      // 注入加速度 (牛顿第三定律)
#pragma omp atomic
                      acc[i * 3] += forceRes.fx / mass_i;
#pragma omp atomic
                      acc[i * 3 + 1] += forceRes.fy / mass_i;
#pragma omp atomic
                      acc[i * 3 + 2] += forceRes.fz / mass_i;

                      if (massJ > 1e-30) {
#pragma omp atomic
                        acc[j * 3] -= forceRes.fx / massJ;
#pragma omp atomic
                        acc[j * 3 + 1] -= forceRes.fy / massJ;
#pragma omp atomic
                        acc[j * 3 + 2] -= forceRes.fz / massJ;
                      }
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
  }
}

} // namespace PDCommon::Contact

// 注册：YAML Type: "NTN"
REGISTER_CONTACT_TYPE(
    NTN, [](const std::string &name,
            std::unique_ptr<PDCommon::Contact::IContactForceLaw> fl) {
      return std::make_unique<PDCommon::Contact::NTNContact>(name,
                                                             std::move(fl));
    })
