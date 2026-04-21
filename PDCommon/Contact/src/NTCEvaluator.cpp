#include "NTCEvaluator.h"
#include "ContactRegistry.h"
#include "ContactSpatialGrid.h"
#include "FieldManager.h"
#include "MechanicalMaterial.h"
#include "PDContext.h"
#include "ParticleManager.h"
#include <cmath>

namespace PDCommon::Contact {

void NTCEvaluator::initialize(const YAML::Node &configNode) {
  if (configNode["PinballRatio"]) {
    pinballRatio_ = configNode["PinballRatio"].as<double>();
  }
}

void NTCEvaluator::onPreEvaluate(PDCommon::Core::PDContext &ctx, double maxDx) {
  auto &fm = ctx.getFieldManager();

  auto *coordsField = fm.getFieldAs<double>("Coords");
  auto *dispField = fm.getFieldAs<double>("Displacement");
  auto *volField = fm.getFieldAs<double>("Volume");
  auto *velField = fm.getFieldAs<double>("Velocity");
  auto *partField = fm.getFieldAs<int>("PartID");

  coords_ = coordsField ? coordsField->dataPtr() : nullptr;
  disp_ = dispField ? dispField->dataPtr() : nullptr;
  vols_ = volField ? volField->dataPtr() : nullptr;
  vel_ = velField ? velField->dataPtr() : nullptr;
  partIDs_ = partField ? partField->dataPtr() : nullptr;

  dim_ = ctx.getDimension();
  thickness_ = ctx.getThickness();
}

std::vector<ContactContext>
NTCEvaluator::evaluate(int i, const ContactSpatialGrid &grid,
                       PDCommon::Core::PDContext &ctx) {

  std::vector<ContactContext> contexts;
  if (!coords_ || !vols_)
    return contexts;

  auto &pm = ctx.getParticleManager();
  const auto &neighborList = ctx.getNeighborList();
  const auto &particles = pm.getAllParticles();

  auto volToDx = [this](double v) -> double {
    return (dim_ == 2) ? std::sqrt(v / thickness_) : std::cbrt(v);
  };

  double xi = coords_[i * 3] + (disp_ ? disp_[i * 3] : 0.0);
  double yi = coords_[i * 3 + 1] + (disp_ ? disp_[i * 3 + 1] : 0.0);
  double zi = coords_[i * 3 + 2] + (disp_ ? disp_[i * 3 + 2] : 0.0);
  double dx_i = volToDx(vols_[i]);

  auto *mat_i = dynamic_cast<PDCommon::Material::MechanicalMaterial *>(
      particles[i].getMaterial());
  double rho_i = mat_i ? mat_i->getDensity() : 1.0;
  double mass_i = rho_i * vols_[i] * massScaleFactor_;
  double sumPenetration = 0.0;
  int numContacts = 0;
  double nx_total = 0.0, ny_total = 0.0, nz_total = 0.0;
  std::vector<std::pair<int, double>> interacting_js;

  grid.forEachNeighbor(xi, yi, zi, [&](int j) {
    if (i == j)
      return;

    double xj = coords_[j * 3] + (disp_ ? disp_[j * 3] : 0.0);
    double yj = coords_[j * 3 + 1] + (disp_ ? disp_[j * 3 + 1] : 0.0);
    double zj = coords_[j * 3 + 2] + (disp_ ? disp_[j * 3 + 2] : 0.0);

    double rx = xi - xj, ry = yi - yj, rz = zi - zj;
    double distSqr = rx * rx + ry * ry + rz * rz;

    if (distSqr > 1e-14) {
      bool isInitialNeighbor = false;
      if (partIDs_ && partIDs_[i] == partIDs_[j]) {
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
        double dx_j = volToDx(vols_[j]);
        double safeDist = ((dx_i + dx_j) / 2.0) * pinballRatio_;

        if (dist < safeDist) {
          double penetration = safeDist - dist;
          // [修复]
          // 移除NTCEvaluator中的法向截断，让底层可以提供大侵入量下的足够反力。
          penetration = std::min(penetration, safeDist * 0.5);

          double weight = penetration; // 使用侵入量作加权权重
          sumPenetration += weight;
          numContacts++;

          nx_total += (rx / dist) * weight;
          ny_total += (ry / dist) * weight;
          nz_total += (rz / dist) * weight;

          interacting_js.push_back({j, weight});
        }
      }
    }
  });

  if (numContacts > 0 && sumPenetration > 1e-14) {
    double inv_sum = 1.0 / sumPenetration;

    double n_len = std::sqrt(nx_total * nx_total + ny_total * ny_total +
                             nz_total * nz_total);
    if (n_len > 1e-10) {
      nx_total /= n_len;
      ny_total /= n_len;
      nz_total /= n_len;
    } else {
      return contexts; // 抵消了，等于没接触
    }

    int j_rep = interacting_js[0].first;
    auto *mat_rep = dynamic_cast<PDCommon::Material::MechanicalMaterial *>(
        particles[j_rep].getMaterial());
    double mass_master = (mat_rep ? mat_rep->getDensity() : 1.0) *
                         vols_[j_rep] * massScaleFactor_;
    double avgPenetration = sumPenetration / numContacts;

    double dx_j_rep = volToDx(vols_[j_rep]);
    double safeDist_rep = ((dx_i + dx_j_rep) / 2.0) * pinballRatio_;

    ContactContext pair;
    pair.i = i;
    pair.j = j_rep;                   // 代表材料属性的主面粒子
    pair.dist = safeDist_rep - avgPenetration; // 等效距离
    pair.safeDist = safeDist_rep;      // 真实的接触检测阈值
    pair.raw_penetration = avgPenetration;
    pair.nx = nx_total;
    pair.ny = ny_total;
    pair.nz = nz_total;
    pair.mass_i = mass_i;
    pair.mass_j = mass_master;
    pair.dx_i = dx_i;
    pair.dx_j = volToDx(vols_[j_rep]);
    // NTC cloud 反向分配权重，先归一化
    for (auto &wp : interacting_js) {
      wp.second *= inv_sum;
    }
    pair.master_weights = interacting_js;

    // 多态隔离：在此处彻底计算出云等效相对速度 (v_i - v_master_equiv)
    if (vel_) {
      double vx_master = 0, vy_master = 0, vz_master = 0;
      for (const auto &wp : interacting_js) {
        vx_master += vel_[wp.first * 3] * wp.second;
        vy_master += vel_[wp.first * 3 + 1] * wp.second;
        vz_master += vel_[wp.first * 3 + 2] * wp.second;
      }
      pair.dvx = vel_[i * 3] - vx_master;
      pair.dvy = vel_[i * 3 + 1] - vy_master;
      pair.dvz = vel_[i * 3 + 2] - vz_master;
    } else {
      pair.dvx = pair.dvy = pair.dvz = 0.0;
    }

    contexts.push_back(pair);
  }

  return contexts;
}

} // namespace PDCommon::Contact

REGISTER_EVALUATOR_TYPE(NTC, NTCEvaluator)
