#include "ThermalStabilizers.h"
#include "FieldManager.h"
#include "NeighborList.h"
#include "ParticleManager.h"
#include "StabilizerRegistry.h"
#include "ThermalMaterial.h"
#include <cmath>
#include <omp.h>

namespace PDCommon::Kernel {

using namespace PDCommon::Core;
using namespace PDCommon::Model;
using namespace PDCommon::Field;
using namespace PDCommon::Material;

// 静态注册 Thermal Stabilizers 到全局工厂中
REGISTER_STABILIZER(Thermal_Silling, SillingStabilizer)
REGISTER_STABILIZER(Thermal_Wan, WanStabilizer)
REGISTER_STABILIZER(Thermal_Zhang, ZhangStabilizer)

// =========================================================================
// SillingStabilizer
// =========================================================================
void SillingStabilizer::preCompute(PDContext &ctx) {
  auto &manager = ctx.getParticleManager();
  const size_t numParticles = manager.getTotalParticles();
  kArr_.assign(numParticles, 0.0);
  const auto &particles = manager.getAllParticles();
#pragma omp parallel for schedule(static)
  for (int i = 0; i < static_cast<int>(numParticles); ++i) {
    auto *mat = dynamic_cast<ThermalMaterial *>(particles[i].getMaterial());
    if (mat)
      kArr_[i] = mat->getConductivity();
  }
}

void SillingStabilizer::applyPenalty(PDContext &ctx) {
  auto &fieldManager = ctx.getFieldManager();
  auto &neighborList = ctx.getNeighborList();
  auto &manager = ctx.getParticleManager();
  const size_t numParticles = manager.getTotalParticles();

  const double horizon = neighborList.getHorizon();
  const double delta4 = horizon * horizon * horizon * horizon;
  const double coeff_base = 6.0 / (3.141592653589793 * delta4);

  const double *coords = fieldManager.getFieldAs<double>("Coords")->dataPtr();
  const double *volumes = fieldManager.getFieldAs<double>("Volume")->dataPtr();
  const double *tempPtr =
      fieldManager.getFieldAs<double>("Temperature")->dataPtr();
  const double *gradPtr =
      fieldManager.getFieldAs<double>("TempGradient")->dataPtr();
  const double *vvPtr = fieldManager.getFieldAs<double>("VHorizon")->dataPtr();
  const double *omegaPtr = neighborList.getBondFieldPtr("InfluenceWeight");
  double *ratePtr = fieldManager.getFieldAs<double>("TempRate")->dataPtr();

#pragma omp parallel for schedule(guided)
  for (int i = 0; i < static_cast<int>(numParticles); ++i) {
    double xi_x = coords[i * 3];
    double xi_y = coords[i * 3 + 1];
    double xi_z = coords[i * 3 + 2];
    double temp_i = tempPtr[i];
    double gx_i = gradPtr[i * 3];
    double gy_i = gradPtr[i * 3 + 1];
    double gz_i = gradPtr[i * 3 + 2];
    double coeff_i = coeff_base * g0_ * kArr_[i] / vvPtr[i];

    double rate_sum = 0.0;
    const int numNeighbors = neighborList.getNeighborCount(i);
    const int *neighbors = neighborList.getNeighborIds(i);
    const int offset = neighbors - neighborList.getNeighborIds(0);

#pragma omp simd
    for (int k_nb = 0; k_nb < numNeighbors; ++k_nb) {
      int j = neighbors[k_nb];
      if (j == -1)
        continue;

      double dx = coords[j * 3] - xi_x;
      double dy = coords[j * 3 + 1] - xi_y;
      double dz = coords[j * 3 + 2] - xi_z;
      double omega_vj = omegaPtr[offset + k_nb] * volumes[j];

      double deltaT_act = tempPtr[j] - temp_i;
      double deltaT_res_plus = deltaT_act - (gx_i * dx + gy_i * dy + gz_i * dz);
      double deltaT_res_minus =
          deltaT_act - (gradPtr[j * 3] * dx + gradPtr[j * 3 + 1] * dy +
                        gradPtr[j * 3 + 2] * dz);

      double coeff_j = coeff_base * g0_ * kArr_[j] / vvPtr[j];
      rate_sum += omega_vj / horizon *
                  (coeff_i * deltaT_res_plus + coeff_j * deltaT_res_minus);
    }
    ratePtr[i] += rate_sum;
  }
}

// =========================================================================
// WanStabilizer
// =========================================================================
void WanStabilizer::preCompute(PDContext &ctx) {
  auto &manager = ctx.getParticleManager();
  const size_t numParticles = manager.getTotalParticles();
  GWanArr_.assign(numParticles, 0.0);

  const auto &particles = manager.getAllParticles();
  auto *shapeInv = ctx.getFieldManager().getFieldAs<double>("ShapeTensorInv");
  if (!shapeInv)
    return;
  const double *shapeInvPtr = shapeInv->dataPtr();

#pragma omp parallel for schedule(static)
  for (int i = 0; i < static_cast<int>(numParticles); ++i) {
    auto *mat = dynamic_cast<ThermalMaterial *>(particles[i].getMaterial());
    if (mat) {
      double k = mat->getConductivity();
      double trace_inv =
          shapeInvPtr[i * 9] + shapeInvPtr[i * 9 + 4] + shapeInvPtr[i * 9 + 8];
      GWanArr_[i] = g0_ * k * trace_inv;
    }
  }
}

void WanStabilizer::applyPenalty(PDContext &ctx) {
  auto &fieldManager = ctx.getFieldManager();
  auto &neighborList = ctx.getNeighborList();
  auto &manager = ctx.getParticleManager();
  const size_t numParticles = manager.getTotalParticles();

  const double *coords = fieldManager.getFieldAs<double>("Coords")->dataPtr();
  const double *volumes = fieldManager.getFieldAs<double>("Volume")->dataPtr();
  const double *tempPtr =
      fieldManager.getFieldAs<double>("Temperature")->dataPtr();
  const double *gradPtr =
      fieldManager.getFieldAs<double>("TempGradient")->dataPtr();
  const double *omegaPtr = neighborList.getBondFieldPtr("InfluenceWeight");
  double *ratePtr = fieldManager.getFieldAs<double>("TempRate")->dataPtr();

#pragma omp parallel for schedule(guided)
  for (int i = 0; i < static_cast<int>(numParticles); ++i) {
    double xi_x = coords[i * 3], xi_y = coords[i * 3 + 1],
           xi_z = coords[i * 3 + 2];
    double temp_i = tempPtr[i];
    double gx_i = gradPtr[i * 3], gy_i = gradPtr[i * 3 + 1],
           gz_i = gradPtr[i * 3 + 2];
    double G_Wan_i = GWanArr_[i];

    double rate_sum = 0.0;
    const int numNeighbors = neighborList.getNeighborCount(i);
    const int *neighbors = neighborList.getNeighborIds(i);
    const int offset = neighbors - neighborList.getNeighborIds(0);

#pragma omp simd
    for (int k_nb = 0; k_nb < numNeighbors; ++k_nb) {
      int j = neighbors[k_nb];
      if (j == -1)
        continue;

      double dx = coords[j * 3] - xi_x;
      double dy = coords[j * 3 + 1] - xi_y;
      double dz = coords[j * 3 + 2] - xi_z;
      double omega_vj = omegaPtr[offset + k_nb] * volumes[j];

      double deltaT_act = tempPtr[j] - temp_i;
      double deltaT_res_plus = deltaT_act - (gx_i * dx + gy_i * dy + gz_i * dz);
      double deltaT_res_minus =
          deltaT_act - (gradPtr[j * 3] * dx + gradPtr[j * 3 + 1] * dy +
                        gradPtr[j * 3 + 2] * dz);

      rate_sum += omega_vj *
                  (G_Wan_i * deltaT_res_plus + GWanArr_[j] * deltaT_res_minus);
    }
    ratePtr[i] += rate_sum;
  }
}

// =========================================================================
// ZhangStabilizer
// =========================================================================
void ZhangStabilizer::preCompute(PDContext &ctx) {
  auto &manager = ctx.getParticleManager();
  const size_t numParticles = manager.getTotalParticles();
  kArr_.assign(numParticles, 0.0);
  const auto &particles = manager.getAllParticles();
#pragma omp parallel for schedule(static)
  for (int i = 0; i < static_cast<int>(numParticles); ++i) {
    auto *mat = dynamic_cast<ThermalMaterial *>(particles[i].getMaterial());
    if (mat)
      kArr_[i] = mat->getConductivity();
  }
}

void ZhangStabilizer::applyPenalty(PDContext &ctx) {
  auto &fieldManager = ctx.getFieldManager();
  auto &neighborList = ctx.getNeighborList();
  auto &manager = ctx.getParticleManager();
  const size_t numParticles = manager.getTotalParticles();

  const double *coords = fieldManager.getFieldAs<double>("Coords")->dataPtr();
  const double *volumes = fieldManager.getFieldAs<double>("Volume")->dataPtr();
  const double *tempPtr =
      fieldManager.getFieldAs<double>("Temperature")->dataPtr();
  const double *gradPtr =
      fieldManager.getFieldAs<double>("TempGradient")->dataPtr();
  const double *vvPtr = fieldManager.getFieldAs<double>("VHorizon")->dataPtr();
  const double *omegaPtr = neighborList.getBondFieldPtr("InfluenceWeight");
  double *ratePtr = fieldManager.getFieldAs<double>("TempRate")->dataPtr();
  const double *shapeInvPtr =
      fieldManager.getFieldAs<double>("ShapeTensorInv")->dataPtr();

#pragma omp parallel for schedule(guided)
  for (int i = 0; i < static_cast<int>(numParticles); ++i) {
    double xi_x = coords[i * 3], xi_y = coords[i * 3 + 1],
           xi_z = coords[i * 3 + 2];
    double temp_i = tempPtr[i];
    double gx_i = gradPtr[i * 3], gy_i = gradPtr[i * 3 + 1],
           gz_i = gradPtr[i * 3 + 2];
    double vv_i = vvPtr[i];
    const double *i_k = &shapeInvPtr[i * 9];
    double k_i = kArr_[i];

    double rate_sum = 0.0;
    const int numNeighbors = neighborList.getNeighborCount(i);
    const int *neighbors = neighborList.getNeighborIds(i);
    const int offset = neighbors - neighborList.getNeighborIds(0);

#pragma omp simd
    for (int k_nb = 0; k_nb < numNeighbors; ++k_nb) {
      int j = neighbors[k_nb];
      if (j == -1)
        continue;

      double dx = coords[j * 3] - xi_x;
      double dy = coords[j * 3 + 1] - xi_y;
      double dz = coords[j * 3 + 2] - xi_z;
      double vj = volumes[j];

      double deltaT_act = tempPtr[j] - temp_i;
      double deltaT_res_plus = deltaT_act - (gx_i * dx + gy_i * dy + gz_i * dz);
      double deltaT_res_minus =
          deltaT_act - (gradPtr[j * 3] * dx + gradPtr[j * 3 + 1] * dy +
                        gradPtr[j * 3 + 2] * dz);

      double px_i = i_k[0] * dx + i_k[1] * dy + i_k[2] * dz;
      double py_i = i_k[3] * dx + i_k[4] * dy + i_k[5] * dz;
      double pz_i = i_k[6] * dx + i_k[7] * dy + i_k[8] * dz;
      double p_dot_p_i = px_i * px_i + py_i * py_i + pz_i * pz_i;

      double omega = omegaPtr[offset + k_nb];
      double o2 = omega * omega;
      double G_Zhang_i = o2 * k_i * p_dot_p_i * vvPtr[j];

      const double *j_k = &shapeInvPtr[j * 9];
      double px_j = j_k[0] * dx + j_k[1] * dy + j_k[2] * dz;
      double py_j = j_k[3] * dx + j_k[4] * dy + j_k[5] * dz;
      double pz_j = j_k[6] * dx + j_k[7] * dy + j_k[8] * dz;
      double p_dot_p_j = px_j * px_j + py_j * py_j + pz_j * pz_j;
      double G_Zhang_j = o2 * kArr_[j] * p_dot_p_j * vv_i;

      rate_sum += g0_ * vj *
                  (G_Zhang_i * deltaT_res_plus + G_Zhang_j * deltaT_res_minus);
    }
    ratePtr[i] += rate_sum;
  }
}

} // namespace PDCommon::Kernel
