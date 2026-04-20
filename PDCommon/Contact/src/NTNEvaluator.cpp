#include "NTNEvaluator.h"
#include "ContactRegistry.h"
#include "ContactSpatialGrid.h"
#include "FieldManager.h"
#include "ParticleManager.h"
#include "MechanicalMaterial.h"
#include "PDContext.h"
#include <cmath>

namespace PDCommon::Contact {

void NTNEvaluator::initialize(const YAML::Node &configNode) {
  if (configNode["PinballRatio"]) {
    pinballRatio_ = configNode["PinballRatio"].as<double>();
  }
}

void NTNEvaluator::onPreEvaluate(PDCommon::Core::PDContext &ctx, double maxDx) {
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

std::vector<ContactContext> NTNEvaluator::evaluate(
    int i, const ContactSpatialGrid &grid, PDCommon::Core::PDContext &ctx) {

  std::vector<ContactContext> contexts;
  if (!coords_ || !vols_) return contexts;

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

  grid.forEachNeighbor(xi, yi, zi, [&](int j) {
    if (i == j) return;

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
          double raw_penetration = safeDist - dist;
          raw_penetration = std::min(raw_penetration, safeDist * 0.5);

          auto *matJ = dynamic_cast<PDCommon::Material::MechanicalMaterial *>(
              particles[j].getMaterial());
          double rhoJ = matJ ? matJ->getDensity() : 1.0;
          double massJ = rhoJ * vols_[j] * massScaleFactor_;

          ContactContext pair;
          pair.i = i;
          pair.j = j;
          pair.dist = dist;
          pair.safeDist = safeDist;
          pair.raw_penetration = raw_penetration;
          pair.nx = rx / dist;
          pair.ny = ry / dist;
          pair.nz = rz / dist;
          pair.mass_i = mass_i;
          pair.mass_j = massJ;
          pair.dx_i = dx_i;
          pair.dx_j = dx_j;

          // 多态隔离：在 Evaluator 这层直接算好相对速度 (v_i - v_j)
          if (vel_) {
              pair.dvx = vel_[i * 3] - vel_[j * 3];
              pair.dvy = vel_[i * 3 + 1] - vel_[j * 3 + 1];
              pair.dvz = vel_[i * 3 + 2] - vel_[j * 3 + 2];
          } else {
              pair.dvx = pair.dvy = pair.dvz = 0.0;
          }

          contexts.push_back(pair);

        }
      }
    }
  });

  return contexts;
}

} // namespace PDCommon::Contact

REGISTER_EVALUATOR_TYPE(NTN, NTNEvaluator)
