#include "KinematicContact.h"
#include "ContactRegistry.h"
#include "FieldManager.h"
#include "Logger.h"
#include "MechanicalMaterial.h"
#include "PDContext.h"
#include "ParticleManager.h"
#include "StringUtils.h"
#include <cmath>
#include <omp.h>
#include <vector>


namespace PDCommon::Contact {

KinematicContact::KinematicContact(const std::string &name)
    : IContactAlgorithm(name) {}

void KinematicContact::buildCellList(const double *coords, const double *disp,
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

int KinematicContact::computeCellHash(double x, double y, double z) const {
  int cx = static_cast<int>(std::floor((x - minBounds_.x()) / cellSize_));
  int cy = static_cast<int>(std::floor((y - minBounds_.y()) / cellSize_));
  int cz = static_cast<int>(std::floor((z - minBounds_.z()) / cellSize_));
  cx = std::max(0, std::min(cx, gridDims_.x() - 1));
  cy = std::max(0, std::min(cy, gridDims_.y() - 1));
  cz = std::max(0, std::min(cz, gridDims_.z() - 1));
  return cx + cy * gridDims_.x() + cz * gridDims_.x() * gridDims_.y();
}

void KinematicContact::initialize(const YAML::Node &configNode) {
  if (configNode["RestitutionCoeff"]) {
    restitutionCoeff_ = configNode["RestitutionCoeff"].as<double>();
  }
  if (configNode["PinballRatio"]) {
    pinballRatio_ = configNode["PinballRatio"].as<double>();
  }
  if (configNode["FrictionCoeff"]) {
    frictionCoeff_ = configNode["FrictionCoeff"].as<double>();
  }

  LOG_INFO(
      "[KinematicContact] Configured MPM Style Contact: RestitutionCoeff=" +
      PDCommon::Utils::StringUtils::toScientific(restitutionCoeff_) +
      ", PinballRatio=" + std::to_string(pinballRatio_) +
      ", FrictionCoeff=" + std::to_string(frictionCoeff_));
}

void KinematicContact::computeContactForce(PDCommon::Core::PDContext &ctx) {
  auto &fm = ctx.getFieldManager();
  auto &pm = ctx.getParticleManager();
  size_t numParticles = pm.getTotalParticles();

  // 从引擎上下文获取真实时间步长
  double dt = ctx.getCurrentDt();
  if (dt < 1e-30) {
    LOG_WARNING("[KinematicContact] ctx.getCurrentDt() returned near-zero! "
                "Using fallback dt=1e-9.");
    dt = 1e-9;
  }

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
  buildCellList(coords, disp, maxDx);

  // 4. OMP 并行：对于每个 slave，计算他受到的"虚拟主面"的接触作用
#pragma omp parallel for schedule(dynamic)
  for (size_t idx = 0; idx < slaveIds_.size(); ++idx) {
    int i = slaveIds_[idx];
    // if (activeStatusPtr && activeStatusPtr[i] == 0)
    //   continue;

    double xi = coords[i * 3] + (disp ? disp[i * 3] : 0.0);
    double yi = coords[i * 3 + 1] + (disp ? disp[i * 3 + 1] : 0.0);
    double zi = coords[i * 3 + 2] + (disp ? disp[i * 3 + 2] : 0.0);
    double dx_i = volToDx(vols[i]);

    auto *mat_i = dynamic_cast<PDCommon::Material::MechanicalMaterial *>(
        particles[i].getMaterial());
    double rho_i = mat_i ? mat_i->getDensity() : 1.0;
    double mass_i = rho_i * vols[i] * massScaleFactor_;

    int cx = static_cast<int>(std::floor((xi - minBounds_.x()) / cellSize_));
    int cy = static_cast<int>(std::floor((yi - minBounds_.y()) / cellSize_));
    int cz = static_cast<int>(std::floor((zi - minBounds_.z()) / cellSize_));

    // -- 收集局部主面信息 --
    double sumPenetration = 0.0;
    int numContacts = 0;
    double nx_total = 0.0, ny_total = 0.0, nz_total = 0.0;
    double vx_master = 0.0, vy_master = 0.0, vz_master = 0.0;

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
              // if (activeStatusPtr && activeStatusPtr[j] == 0) {
              //   j = next_[j];
              //   continue;
              // }

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
                  double safeDist = ((dx_i + dx_j) / 2.0) * pinballRatio_;

                  if (dist < safeDist) {
                    double penetration = safeDist - dist;
                    // 【修复2】穿透截断保护：限制最大穿透量不超过安全距离的50%
                    // 防止粒子几乎重合时力爆炸和法线方向翻转
                    penetration = std::min(penetration, safeDist * 0.5);
                    double weight = penetration; // 这里权重取侵入量
                    sumPenetration += weight;
                    numContacts++;

                    nx_total += (rx / dist) * weight;
                    ny_total += (ry / dist) * weight;
                    nz_total += (rz / dist) * weight;

                    vx_master += vel[j * 3] * weight;
                    vy_master += vel[j * 3 + 1] * weight;
                    vz_master += vel[j * 3 + 2] * weight;

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

      // 取代表 master 质量（同材料粒子质量相同，取第一个即可）
      int j_rep = interacting_js[0].first;
      auto *mat_rep = dynamic_cast<PDCommon::Material::MechanicalMaterial *>(
          particles[j_rep].getMaterial());
      double mass_master = (mat_rep ? mat_rep->getDensity() : 1.0) *
                           vols[j_rep] * massScaleFactor_;

      // 等效质量：确保 slave 和 master 按质量比分担修正
      // 钨(重)撞钢(轻)：钨受小修正，钢受大修正 — 物理正确
      double m_eff = (mass_i * mass_master) / (mass_i + mass_master);

      // 1. 位置修正法向力
      // 加权平均穿透量（与法线、速度的加权平均一致）
      double avgPenetration = sumPenetration / numContacts;
      double f_pos = m_eff * avgPenetration / (dt * dt);

      // 2. 速度阻尼力
      double dvx = vel[i * 3] - vx_master;
      double dvy = vel[i * 3 + 1] - vy_master;
      double dvz = vel[i * 3 + 2] - vz_master;
      double v_rel_n = dvx * nx_total + dvy * ny_total + dvz * nz_total;

      double f_damp = 0.0;
      if (v_rel_n < 0.0) { // 逼近
        f_damp = (1.0 + restitutionCoeff_) * m_eff * (-v_rel_n) / dt;
        f_damp = std::min(f_damp, 2.0 * f_pos); // 限幅保护
      }

      double force_mag = f_pos + f_damp;
      double fx = force_mag * nx_total;
      double fy = force_mag * ny_total;
      double fz = force_mag * nz_total;

      // 3. 库伦滑动摩擦机制 (Coulomb Friction)
      if (frictionCoeff_ > 1e-6) {
        // 算出切向的相对滑移速度矢量: v_t = dv - (dv * n) * n
        double v_tx = dvx - v_rel_n * nx_total;
        double v_ty = dvy - v_rel_n * ny_total;
        double v_tz = dvz - v_rel_n * nz_total;
        double v_t_mag = std::sqrt(v_tx * v_tx + v_ty * v_ty + v_tz * v_tz);

        if (v_t_mag > 1e-10) {
          // 最大基础摩擦力 F = mu * F_n
          double f_fric_mag = frictionCoeff_ * force_mag;
          
          // [关键限幅保护] 摩擦力不能在一个积分步把相对切向速度减到反向导致震荡
          // 彻底将其刹车所需的上限力 (冲量守恒理论)
          double max_fric = m_eff * v_t_mag / dt;
          f_fric_mag = std::min(f_fric_mag, max_fric);

          // 摩擦力方向永远与瞬时切向速度相反
          fx += -f_fric_mag * (v_tx / v_t_mag);
          fy += -f_fric_mag * (v_ty / v_t_mag);
          fz += -f_fric_mag * (v_tz / v_t_mag);
        }
      }

      // 注入从面
      acc[i * 3] += fx / mass_i;
      acc[i * 3 + 1] += fy / mass_i;
      acc[i * 3 + 2] += fz / mass_i;

      // 按权重反向注入主面（用力除以各 master 粒子质量）
      for (const auto &pair : interacting_js) {
        int j = pair.first;
        double w = pair.second * inv_sum;
        auto *mat_j = dynamic_cast<PDCommon::Material::MechanicalMaterial *>(
            particles[j].getMaterial());
        double mass_j =
            (mat_j ? mat_j->getDensity() : 1.0) * vols[j] * massScaleFactor_;
        if (mass_j > 1e-30) {
#pragma omp atomic
          acc[j * 3] += -fx * w / mass_j;
#pragma omp atomic
          acc[j * 3 + 1] += -fy * w / mass_j;
#pragma omp atomic
          acc[j * 3 + 2] += -fz * w / mass_j;
        }
      }
    }
  }
}

} // namespace PDCommon::Contact

REGISTER_CONTACT_TYPE(Kinematic, [](const std::string &name, std::unique_ptr<PDCommon::Contact::IContactForceLaw> fl) {
  return std::make_unique<PDCommon::Contact::KinematicContact>(name);
})
