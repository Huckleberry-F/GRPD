#include "NodeSurfaceContact.h"
#include "PDContext.h"
#include "StringUtils.h"
#include "FieldManager.h"
#include "Logger.h"
#include "NeighborList.h"
#include <omp.h>
#include <cmath>

namespace PDCommon::Contact {

void NodeSurfaceContact::computeContactForce(PDCommon::Core::PDContext &ctx) {
  auto &fm = ctx.getFieldManager();
  auto *coordsField = fm.getFieldAs<double>("Coords");
  auto *dispField = fm.getFieldAs<double>("Displacement");
  auto *activeField = fm.getFieldAs<int>("ActiveStatus");
  auto *volField = fm.getFieldAs<double>("Volume");
  auto *massField = fm.getFieldAs<double>("Mass");
  auto *accField = fm.getFieldAs<double>("Acceleration");
  auto *velField = fm.getFieldAs<double>("Velocity");
  auto &neighborList = ctx.getNeighborList();

  if (!coordsField || !volField || !massField || !accField) {
    LOG_WARNING("[" + name_ + "] Missing core kinematics fields!");
    return;
  }

  const double *coords = coordsField->dataPtr();
  const double *disp = dispField ? dispField->dataPtr() : nullptr;
  const int *activeStatus = activeField ? activeField->dataPtr() : nullptr;
  const double *vols = volField->dataPtr();
  const double *mass = massField->dataPtr();
  const double *vel = velField ? velField->dataPtr() : nullptr;
  double *acc = accField->dataPtr();

  // 2D/3D dx
  const int dim = ctx.getDimension();
  const double thickness = ctx.getThickness();
  auto volToDx = [dim, thickness](double v) -> double {
    return (dim == 2) ? std::sqrt(v / thickness) : std::cbrt(v);
  };

  double maxDx = 0.0;
  for (int i : masterIds_) {
    if (activeStatus && activeStatus[i] == 0) continue;
    double dx = volToDx(vols[i]);
    if (dx > maxDx) maxDx = dx;
  }

  onPreContact(ctx, maxDx);

  grid_.build(masterIds_, coords, disp, activeStatus, maxDx, ctx.getParticleManager().getAllParticles().size());

#pragma omp parallel
  {
    std::vector<int> localMasterCandidates;
    localMasterCandidates.reserve(128);

#pragma omp for schedule(dynamic, 64)
    for (size_t idx = 0; idx < slaveIds_.size(); ++idx) {
      int i = slaveIds_[idx];
      if (activeStatus && activeStatus[i] == 0) continue;

      double xi = coords[i * 3] + (disp ? disp[i * 3] : 0.0);
      double yi = coords[i * 3 + 1] + (disp ? disp[i * 3 + 1] : 0.0);
      double zi = coords[i * 3 + 2] + (disp ? disp[i * 3 + 2] : 0.0);
      double dx_i = volToDx(vols[i]);
      double searchRadius = (dx_i + maxDx) * 0.5 * 1.5;

      int cur_cx = static_cast<int>(std::floor((xi - grid_.minBounds_.x()) / grid_.cellSize_));
      int cur_cy = static_cast<int>(std::floor((yi - grid_.minBounds_.y()) / grid_.cellSize_));
      int cur_cz = static_cast<int>(std::floor((zi - grid_.minBounds_.z()) / grid_.cellSize_));

      localMasterCandidates.clear();

      for (int dz = -1; dz <= 1; ++dz) {
        for (int dy = -1; dy <= 1; ++dy) {
          for (int dx_c = -1; dx_c <= 1; ++dx_c) {
            int ncx = cur_cx + dx_c;
            int ncy = cur_cy + dy;
            int ncz = cur_cz + dz;

            if (ncx < 0 || ncx >= grid_.gridDims_.x() ||
                ncy < 0 || ncy >= grid_.gridDims_.y() ||
                ncz < 0 || ncz >= grid_.gridDims_.z())
              continue;

            int hash = ncx + ncy * grid_.gridDims_.x() + ncz * grid_.gridDims_.x() * grid_.gridDims_.y();
            int j = grid_.head_[hash];
            while (j != -1) {
              if (activeStatus && activeStatus[j] == 0) {
                j = grid_.next_[j];
                continue;
              }

              double xj = coords[j * 3] + (disp ? disp[j * 3] : 0.0);
              double yj = coords[j * 3 + 1] + (disp ? disp[j * 3 + 1] : 0.0);
              double zj = coords[j * 3 + 2] + (disp ? disp[j * 3 + 2] : 0.0);

              double distSqr = (xi - xj) * (xi - xj) +
                               (yi - yj) * (yi - yj) +
                               (zi - zj) * (zi - zj);
              
              if (distSqr < searchRadius * searchRadius) {
                localMasterCandidates.push_back(j);
              }
              j = grid_.next_[j];
            }
          }
        }
      }

      if (localMasterCandidates.empty()) continue;

      double nx_total = 0.0, ny_total = 0.0, nz_total = 0.0;
      double sumPenetration = 0.0;
      double maxPenetration = 0.0;
      double mass_master_eff = 0.0;
      double vx_master = 0.0, vy_master = 0.0, vz_master = 0.0;

      for (int j : localMasterCandidates) {
        double xj = coords[j * 3] + (disp ? disp[j * 3] : 0.0);
        double yj = coords[j * 3 + 1] + (disp ? disp[j * 3 + 1] : 0.0);
        double zj = coords[j * 3 + 2] + (disp ? disp[j * 3 + 2] : 0.0);

        double distSqr = (xi - xj) * (xi - xj) +
                         (yi - yj) * (yi - yj) +
                         (zi - zj) * (zi - zj);

        bool isInitialNeighbor = false;
        if (checkInitialNeighbor_) {
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
            double weight = penetration; // ��������ΪȨ��
            sumPenetration += weight;
            if (penetration > maxPenetration) {
              maxPenetration = penetration;
            }

            double nx_j = (xi - xj) / (dist + 1e-16);
            double ny_j = (yi - yj) / (dist + 1e-16);
            double nz_j = (zi - zj) / (dist + 1e-16);

            nx_total += nx_j * weight;
            ny_total += ny_j * weight;
            nz_total += nz_j * weight;

            mass_master_eff += mass[j] * weight;
            if (vel) {
              vx_master += vel[j * 3] * weight;
              vy_master += vel[j * 3 + 1] * weight;
              vz_master += vel[j * 3 + 2] * weight;
            }
          }
        }
      }

      if (sumPenetration > 1e-14) {
        double inv_sum = 1.0 / sumPenetration;

        double n_len = std::sqrt(nx_total * nx_total + ny_total * ny_total +
                                 nz_total * nz_total);
        if (n_len > 1e-10) {
          nx_total /= n_len;
          ny_total /= n_len;
          nz_total /= n_len;
        } else {
          nx_total = 0.0; ny_total = 1.0; nz_total = 0.0;
        }

        mass_master_eff *= inv_sum;
        vx_master *= inv_sum;
        vy_master *= inv_sum;
        vz_master *= inv_sum;

        SurfaceContext surf;
        surf.slave_id = i;
        surf.maxPenetration = maxPenetration;
        surf.nx = nx_total;
        surf.ny = ny_total;
        surf.nz = nz_total;
        surf.mass_slave = mass[i];
        surf.m_master_eff = std::max(mass_master_eff, mass[i]);
        surf.vx_master = vx_master;
        surf.vy_master = vy_master;
        surf.vz_master = vz_master;
        surf.vel = vel;

        SurfaceForceResult result = computeSurfaceForce(surf);

        if (result.valid) {
          double fx = result.fx + result.ft_x;
          double fy = result.fy + result.ft_y;
          double fz = result.fz + result.ft_z;

          double mass_i = mass[i];
#pragma omp atomic
          acc[i * 3] += fx / mass_i;
#pragma omp atomic
          acc[i * 3 + 1] += fy / mass_i;
#pragma omp atomic
          acc[i * 3 + 2] += fz / mass_i;

          for (int j : localMasterCandidates) {
            double distSqr = (xi - (coords[j * 3] + (disp ? disp[j * 3] : 0.0))) * (xi - (coords[j * 3] + (disp ? disp[j * 3] : 0.0))) +
                             (yi - (coords[j * 3 + 1] + (disp ? disp[j * 3 + 1] : 0.0))) * (yi - (coords[j * 3 + 1] + (disp ? disp[j * 3 + 1] : 0.0))) +
                             (zi - (coords[j * 3 + 2] + (disp ? disp[j * 3 + 2] : 0.0))) * (zi - (coords[j * 3 + 2] + (disp ? disp[j * 3 + 2] : 0.0)));
            double dist = std::sqrt(distSqr);
            double dx_j = volToDx(vols[j]);
            double safeDist = (dx_i + dx_j) / 2.0;

            if (dist < safeDist) {
              double penetration = safeDist - dist;
              double w = (penetration * inv_sum);

              double mass_j = mass[j];
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
  }
}

} // namespace PDCommon::Contact
