#include "KinematicContact.h"
#include "ContactRegistry.h"
#include "FieldManager.h"
#include "Logger.h"
#include "StringUtils.h"
#include "MechanicalMaterial.h"
#include "PDContext.h"
#include "ParticleManager.h"
#include <cmath>
#include <omp.h>
#include <vector>

namespace PDCommon::Contact {

KinematicContact::KinematicContact(const std::string &name)
    : NodeNodeContact(name) {}

void KinematicContact::initialize(const YAML::Node &configNode) {
  if (configNode["RestitutionCoeff"]) {
    restitutionCoeff_ = configNode["RestitutionCoeff"].as<double>();
  }
  if (configNode["DtEst"]) {
    dt_est_ = configNode["DtEst"].as<double>();
  }

  LOG_INFO(
      "[KinematicContact] Configured MPM Style Contact: RestitutionCoeff=" +
      PDCommon::Utils::StringUtils::toScientific(restitutionCoeff_) + ", DtEst=" + std::to_string(dt_est_));
}

void KinematicContact::computeContactForce(PDCommon::Core::PDContext &ctx) {
  auto &fm = ctx.getFieldManager();
  auto &pm = ctx.getParticleManager();
  size_t numParticles = pm.getTotalParticles();

  // 1. 获取必需的场
  auto *activeField = fm.getFieldAs<int>("ActiveStatus");
  auto *coordsField = fm.getFieldAs<double>("Coords");
  auto *dispField = fm.getFieldAs<double>("Displacement");
  auto *volField = fm.getFieldAs<double>("Volume");
  auto *accField = fm.getFieldAs<double>("Acceleration");
  auto *velField = fm.getFieldAs<double>("Velocity");
  auto *partField = fm.getFieldAs<int>("PartID");

  if (!coordsField || !volField || !accField || !velField)
    return;

  const int *activeStatusPtr = activeField ? activeField->dataPtr() : nullptr;
  const double *coords = coordsField->dataPtr();
  const double *disp = dispField ? dispField->dataPtr() : nullptr;
  const double *vols = volField->dataPtr();
  double *acc = accField->dataPtr();

  // 2D/3D dx
  const int dim = ctx.getDimension();
  const double thickness = ctx.getThickness();
  auto volToDx = [dim, thickness](double v) -> double {
    return (dim == 2) ? std::sqrt(v / thickness) : std::cbrt(v);
  };
  const double *vel = velField->dataPtr();
  const int *partIDs = partField ? partField->dataPtr() : nullptr;
  const auto &neighborList = ctx.getNeighborList();
  const auto &particles = pm.getAllParticles();

  // 2. 计算 maxDx 用于构建网格
  double maxVol = 0.0;
  for (int i : masterIds_) {
    if (activeStatusPtr && activeStatusPtr[i] == 0)
      continue;
    if (vols[i] > maxVol)
      maxVol = vols[i];
  }
  if (maxVol <= 0.0)
    return;
  double maxDx = volToDx(maxVol);

  // 3. 构建空间网格
  if (next_.size() < numParticles) {
    next_.resize(numParticles, -1);
  }
  buildCellList(coords, disp, activeStatusPtr, maxDx);

  // 4. OMP 并行：对于每个 slave，计算他受到的"虚拟主面"的接触作用
#pragma omp parallel for schedule(dynamic)
  for (size_t idx = 0; idx < slaveIds_.size(); ++idx) {
    int i = slaveIds_[idx];
    if (activeStatusPtr && activeStatusPtr[i] == 0)
      continue;

    double xi = coords[i * 3] + (disp ? disp[i * 3] : 0.0);
    double yi = coords[i * 3 + 1] + (disp ? disp[i * 3 + 1] : 0.0);
    double zi = coords[i * 3 + 2] + (disp ? disp[i * 3 + 2] : 0.0);
    double dx_i = volToDx(vols[i]);

    auto *mat_i = dynamic_cast<PDCommon::Material::MechanicalMaterial *>(
        particles[i].getMaterial());
    double rho_i = mat_i ? mat_i->getDensity() : 1.0;
    double mass_i = rho_i * vols[i];

    int cx = static_cast<int>(std::floor((xi - minBounds_.x()) / cellSize_));
    int cy = static_cast<int>(std::floor((yi - minBounds_.y()) / cellSize_));
    int cz = static_cast<int>(std::floor((zi - minBounds_.z()) / cellSize_));

    // -- 收集局部主面信息 --
    double sumPenetration = 0.0;
    double maxPenetration = 0.0;
    double nx_total = 0.0, ny_total = 0.0, nz_total = 0.0;
    double vx_master = 0.0, vy_master = 0.0, vz_master = 0.0;
    double m_master_eff = 0.0;

    // 用于记录互相作用的 j 以及权重，以反向分布力
    std::vector<std::pair<int, double>> interacting_js;

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
                    double penetration = safeDist - dist;
                    double weight = penetration; // 这里权重取侵入量
                    sumPenetration += weight;
                    if (penetration > maxPenetration)
                      maxPenetration = penetration;

                    nx_total += (rx / dist) * weight;
                    ny_total += (ry / dist) * weight;
                    nz_total += (rz / dist) * weight;

                    vx_master += vel[j * 3] * weight;
                    vy_master += vel[j * 3 + 1] * weight;
                    vz_master += vel[j * 3 + 2] * weight;

                    auto *mat_j =
                        dynamic_cast<PDCommon::Material::MechanicalMaterial *>(
                            particles[j].getMaterial());
                    double rho_j = mat_j ? mat_j->getDensity() : 1.0;
                    m_master_eff += rho_j * vols[j];

                    interacting_js.push_back({j, weight});
                  }
                }
              }
            }
            j = next_[j];
          }
        }
      }
    }

    if (sumPenetration > 1e-14) {
      // 归一化综合主面属性
      double inv_sum = 1.0 / sumPenetration;
      vx_master *= inv_sum;
      vy_master *= inv_sum;
      vz_master *= inv_sum;

      double n_len = std::sqrt(nx_total * nx_total + ny_total * ny_total +
                               nz_total * nz_total);
      if (n_len > 1e-10) {
        nx_total /= n_len;
        ny_total /= n_len;
        nz_total /= n_len;
      }

      // 计算和主面的等效质量
      m_master_eff = std::max(m_master_eff, mass_i); // 粗略估算局部等效质量
      double m_eff = (mass_i * m_master_eff) / (mass_i + m_master_eff);

      // 1. 位置修正法向力
      double f_pos_mag = m_eff * maxPenetration / (dt_est_ * dt_est_);

      // 2. 速度阻尼力
      double dvx = vel[i * 3] - vx_master;
      double dvy = vel[i * 3 + 1] - vy_master;
      double dvz = vel[i * 3 + 2] - vz_master;
      double v_rel_n = dvx * nx_total + dvy * ny_total + dvz * nz_total;

      double f_damp_mag = 0.0;
      if (v_rel_n < 0.0) { // 逼近
        f_damp_mag = (1.0 + restitutionCoeff_) * m_eff * (-v_rel_n) / dt_est_;
        // 限幅：防止意外速度过大产生爆炸力
        f_damp_mag = std::min(f_damp_mag, 2.0 * f_pos_mag);
      }

      double force_mag = f_pos_mag + f_damp_mag;
      double fx = force_mag * nx_total;
      double fy = force_mag * ny_total;
      double fz = force_mag * nz_total;

      // 注入从面
      if (mass_i > 1e-30) {
        acc[i * 3] += fx / mass_i;
        acc[i * 3 + 1] += fy / mass_i;
        acc[i * 3 + 2] += fz / mass_i;
      }

      // 按权重反向注入主面
      for (const auto &pair : interacting_js) {
        int j = pair.first;
        double w = pair.second * inv_sum;
        auto *mat_j = dynamic_cast<PDCommon::Material::MechanicalMaterial *>(
            particles[j].getMaterial());
        double mass_j = (mat_j ? mat_j->getDensity() : 1.0) * vols[j];
        if (mass_j > 1e-30) {
          double fj_x = -fx * w;
          double fj_y = -fy * w;
          double fj_z = -fz * w;
#pragma omp atomic
          acc[j * 3] += fj_x / mass_j;
#pragma omp atomic
          acc[j * 3 + 1] += fj_y / mass_j;
#pragma omp atomic
          acc[j * 3 + 2] += fj_z / mass_j;
        }
      }
    }
  }
}

} // namespace PDCommon::Contact

REGISTER_CONTACT_TYPE(KinematicContact, [](const std::string &name) {
  return std::make_unique<PDCommon::Contact::KinematicContact>(name);
})
