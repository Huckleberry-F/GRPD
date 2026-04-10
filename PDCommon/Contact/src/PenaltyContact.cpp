#include "PenaltyContact.h"
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

PenaltyContact::PenaltyContact(const std::string &name)
    : IContactAlgorithm(name) {}

void PenaltyContact::initialize(const YAML::Node &configNode) {
  if (configNode["PenaltyFactor"]) {
    penaltyFactor_ = configNode["PenaltyFactor"].as<double>();
  }
  // 向后兼容旧字段
  if (configNode["PenaltyStiffness"]) {
    penaltyStiffness_ = configNode["PenaltyStiffness"].as<double>();
  }
  if (configNode["PinballRatio"]) {
    pinballRatio_ = configNode["PinballRatio"].as<double>();
  }
  LOG_INFO("[PenaltyContact] Configured: Factor = " +
           std::to_string(penaltyFactor_) +
           ", Pinball = " + std::to_string(pinballRatio_));
}

inline int PenaltyContact::computeCellHash(double x, double y, double z) const {
  int cx = static_cast<int>(std::floor((x - minBounds_.x()) / cellSize_));
  int cy = static_cast<int>(std::floor((y - minBounds_.y()) / cellSize_));
  int cz = static_cast<int>(std::floor((z - minBounds_.z()) / cellSize_));

  // Clamp to boundaries safely to prevent array out-of-bounds
  cx = std::max(0, std::min(cx, gridDims_.x() - 1));
  cy = std::max(0, std::min(cy, gridDims_.y() - 1));
  cz = std::max(0, std::min(cz, gridDims_.z() - 1));

  return cx + cy * gridDims_.x() + cz * gridDims_.x() * gridDims_.y();
}

void PenaltyContact::buildCellList(const double *coords, const double *disp,
                                   const int *isSurface,
                                   const int *activeStatus, double maxDx) {
  cellSize_ =
      maxDx * 1.05; // 网格稍微大于最大粒子尺寸，确保 27 个邻居内能涵盖所有相交

  // 1. 寻找 Bounding Box（只针对存在的 Master 表面粒子）
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

  // 防御性补丁：防止平面网格 Z 维度为 0 导致溢出
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

  // 2. 串行光速哈希入桶 (只哈希 Master 点，因为只能由 Master 当被查的桶)
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

void PenaltyContact::computeContactForce(PDCommon::Core::PDContext &ctx) {
  auto &fm = ctx.getFieldManager();
  auto &pm = ctx.getParticleManager();
  size_t numParticles = pm.getTotalParticles();

  // 1. 提取所有必需的工作场
  auto *isSurfField = fm.getFieldAs<int>("IsSurface");
  auto *activeField = fm.getFieldAs<int>("ActiveStatus");
  auto *coordsField = fm.getFieldAs<double>("Coords");
  auto *dispField = fm.getFieldAs<double>("Displacement"); // 获取当前位移
  auto *volField = fm.getFieldAs<double>("Volume");
  auto *accField = fm.getFieldAs<double>("Acceleration");

  // 【新增提取】：获取 PartID 与 NeighborList
  auto *partField = fm.getFieldAs<int>("PartID");
  const int *partIDs = partField ? partField->dataPtr() : nullptr;
  const auto &neighborList = ctx.getNeighborList();

  if (!isSurfField || !coordsField || !volField || !accField)
    return;

  const int *isSurf = isSurfField->dataPtr();
  const int *active = activeField ? activeField->dataPtr() : nullptr;
  const double *coords = coordsField->dataPtr();
  const double *disp = dispField ? dispField->dataPtr() : nullptr;
  const double *vols = volField->dataPtr();
  double *acc = accField->dataPtr();

  // 2. 预计算 maxDx 用于构网
  double maxVol = 0.0;
  for (int i : masterIds_) {
    if (active && active[i] == 0)
      continue;
    if (vols[i] > maxVol)
      maxVol = vols[i];
  }
  for (int i : slaveIds_) {
    if (active && active[i] == 0)
      continue;
    if (vols[i] > maxVol)
      maxVol = vols[i];
  }

  if (maxVol <= 0.0)
    return; // 无活跃粒子

  double maxDx = std::cbrt(maxVol);

  // -------------------------------------------------------------------------
  // [新增]: 基于材料体积模量自动估算惩罚刚度 (Penalty Stiffness)
  // -------------------------------------------------------------------------
  if (penaltyStiffness_ < 0.0) {
    double masterBulk = 1.0e5; // 默认 100 GPa
    double slaveBulk = 1.0e5;
    
    const auto& particles = pm.getAllParticles();
    if (!masterIds_.empty()) {
      auto *matBase = particles[masterIds_[0]].getMaterial();
      auto *mechMat = dynamic_cast<PDCommon::Material::MechanicalMaterial *>(matBase);
      if (mechMat) masterBulk = mechMat->getBulkModulus();
    }
    if (!slaveIds_.empty()) {
      auto *matBase = particles[slaveIds_[0]].getMaterial();
      auto *mechMat = dynamic_cast<PDCommon::Material::MechanicalMaterial *>(matBase);
      if (mechMat) slaveBulk = mechMat->getBulkModulus();
    }
    
    // 取两者中最软的体积模量作为基准接触刚度
    double minK = std::min(masterBulk, slaveBulk);
    // 经验公式: K_contact = 惩罚系数 * min(K_bulk) * 特征长度 * 阻抗匹配常数
    penaltyStiffness_ = penaltyFactor_ * minK * maxDx;
    LOG_INFO(
        "[PenaltyContact] Auto-Computed PenaltyStiffness: " +
        std::to_string(penaltyStiffness_) + " (Factor: " +
        std::to_string(penaltyFactor_) + ")");
  }
  // -------------------------------------------------------------------------

  // 动态重新分配 next_
  if (next_.size() < numParticles) {
    next_.resize(numParticles, -1);
  }

  // 3. 构建仅含 Master 点的空间网格 (DynamicCellList)
  buildCellList(coords, disp, isSurf, active, maxDx);

  const auto &particles = pm.getAllParticles();

  // 4. Slave 向 Master 发起 OMP 并行检索并注入接触力
#pragma omp parallel for schedule(dynamic)
  for (size_t idx = 0; idx < slaveIds_.size(); ++idx) {
    int i = slaveIds_[idx];
    if (active && active[i] == 0)
      continue;

    double xi = coords[i * 3] + (disp ? disp[i * 3] : 0.0);
    double yi = coords[i * 3 + 1] + (disp ? disp[i * 3 + 1] : 0.0);
    double zi = coords[i * 3 + 2] + (disp ? disp[i * 3 + 2] : 0.0);

    // 计算自己的等效半径 dx_i
    double dx_i = std::cbrt(vols[i]);

    // 提取物理密度
    auto *mat = dynamic_cast<PDCommon::Material::MechanicalMaterial *>(
        particles[i].getMaterial());
    double rho = mat ? mat->getDensity() : 1.0;
    double mass_i = rho * vols[i];

    int cx = static_cast<int>(std::floor((xi - minBounds_.x()) / cellSize_));
    int cy = static_cast<int>(std::floor((yi - minBounds_.y()) / cellSize_));
    int cz = static_cast<int>(std::floor((zi - minBounds_.z()) / cellSize_));

    double fx = 0.0, fy = 0.0, fz = 0.0;

    // 搜寻 27 个相邻格
    for (int offset_x = -1; offset_x <= 1; ++offset_x) {
      for (int offset_y = -1; offset_y <= 1; ++offset_y) {
        for (int offset_z = -1; offset_z <= 1; ++offset_z) {
          int ncx = cx + offset_x;
          int ncy = cy + offset_y;
          int ncz = cz + offset_z;

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
              double xj = coords[j * 3] + (disp ? disp[j * 3] : 0.0);
              double yj = coords[j * 3 + 1] + (disp ? disp[j * 3 + 1] : 0.0);
              double zj = coords[j * 3 + 2] + (disp ? disp[j * 3 + 2] : 0.0);

              double rx = xi - xj;
              double ry = yi - yj;
              double rz = zi - zj;

              double distSqr = rx * rx + ry * ry + rz * rz;
              if (distSqr > 1e-14) { // 防止完全重叠导致除以0

                // 【新增过滤】：如果是同一 Part，检查是否为初始邻居以防误排斥
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
                double dx_j = std::cbrt(vols[j]);
                double safeDist = (dx_i + dx_j) / 2.0;

                if (dist < safeDist) {
                  double raw_penetration = safeDist - dist;
                  double effective_penetration =
                      std::min(raw_penetration, pinballRatio_ * safeDist);
                  double forceMagnitude =
                      penaltyStiffness_ * effective_penetration;

                  // n 的方向：从 j 指向 i
                  double nx = rx / dist;
                  double ny = ry / dist;
                  double nz = rz / dist;

                  // 注入从面
                  fx += forceMagnitude * nx;
                  fy += forceMagnitude * ny;
                  fz += forceMagnitude * nz;

                  // 当场原子注入主面力
                  auto *mat_j =
                      dynamic_cast<PDCommon::Material::MechanicalMaterial *>(
                          particles[j].getMaterial());
                  double rho_j = mat_j ? mat_j->getDensity() : 1.0;
                  double mass_j = rho_j * vols[j];
                  if (mass_j > 0.0) {
#pragma omp atomic
                    acc[j * 3] -= (forceMagnitude * nx) / mass_j;
#pragma omp atomic
                    acc[j * 3 + 1] -= (forceMagnitude * ny) / mass_j;
#pragma omp atomic
                    acc[j * 3 + 2] -= (forceMagnitude * nz) / mass_j;
                  }
                }
              }
            }
            j = next_[j];
          }
        }
      }
    }

    if (mass_i > 0.0) {
      acc[i * 3] += fx / mass_i;
      acc[i * 3 + 1] += fy / mass_i;
      acc[i * 3 + 2] += fz / mass_i;
    }
  }
}

} // namespace PDCommon::Contact

REGISTER_CONTACT_TYPE(PenaltyContact, [](const std::string &name) {
  return std::make_unique<PDCommon::Contact::PenaltyContact>(name);
})
