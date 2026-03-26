#include "MechanicalStabilizers.h"
#include "FieldManager.h"
#include "FieldRegistry.h"
#include "Logger.h"
#include "MechanicalMaterial.h"
#include "NeighborList.h"
#include "ParticleManager.h"
#include <cmath>
#include <omp.h>

namespace PDCommon::Kernel {

using namespace PDCommon::Core;
using namespace PDCommon::Field;
using namespace PDCommon::Model;
using namespace PDCommon::Material;

// 静态注册 Mechanical Stabilizers 到全局工厂中
REGISTER_STABILIZER(Mechanical_Silling, MechanicalSillingStabilizer)
REGISTER_STABILIZER(Mechanical_Wan, MechanicalWanStabilizer)
REGISTER_STABILIZER(Mechanical_Zhang, MechanicalZhangStabilizer)

// =========================================================================
// 抽取通用缓存数组辅助函数 (提取体积模量)
// =========================================================================
static std::vector<double> ExtractBulkModulusArray(ParticleManager &manager) {
  const size_t numParticles = manager.getTotalParticles();
  std::vector<double> bulkArr(numParticles, 0.0);
  const auto &particles = manager.getAllParticles();
#pragma omp parallel for schedule(static)
  for (int i = 0; i < static_cast<int>(numParticles); ++i) {
    auto *mat = dynamic_cast<MechanicalMaterial *>(particles[i].getMaterial());
    if (mat) {
      bulkArr[i] = mat->getBulkModulus();
    }
  }
  return bulkArr;
}

// =========================================================================
// 抽取拉梅常数数组辅助函数 (提取 lambda 和 mu)
// =========================================================================
static void ExtractLameParameters(ParticleManager &manager,
                                  std::vector<double> &lambdaArr,
                                  std::vector<double> &muArr) {
  const size_t numParticles = manager.getTotalParticles();
  lambdaArr.assign(numParticles, 0.0);
  muArr.assign(numParticles, 0.0);
  const auto &particles = manager.getAllParticles();
#pragma omp parallel for schedule(static)
  for (int i = 0; i < static_cast<int>(numParticles); ++i) {
    auto *mat = dynamic_cast<MechanicalMaterial *>(particles[i].getMaterial());
    if (mat) {
      double E = mat->getYoungsModulus();
      double nu = mat->getPoissonsRatio();
      if (nu >= 0.5)
        nu = 0.4999;
      lambdaArr[i] = (E * nu) / ((1.0 + nu) * (1.0 - 2.0 * nu));
      muArr[i] = E / (2.0 * (1.0 + nu));
    }
  }
}

// =========================================================================
// 抽取密度数组辅助函数
// =========================================================================
static std::vector<double> ExtractDensityArray(ParticleManager &manager) {
  const size_t numParticles = manager.getTotalParticles();
  std::vector<double> rhoArr(numParticles, 0.0);
  const auto &particles = manager.getAllParticles();
#pragma omp parallel for schedule(static)
  for (int i = 0; i < static_cast<int>(numParticles); ++i) {
    auto *mat = dynamic_cast<MechanicalMaterial *>(particles[i].getMaterial());
    if (mat) {
      rhoArr[i] = mat->getDensity();
    }
  }
  return rhoArr;
}

// Helper: Compute z_i = Du - (F_i - I)*Dx
static inline void ComputeNonAffineDisp(double du_x, double du_y, double du_z,
                                        double dx, double dy, double dz,
                                        const double *F_i, double &zx,
                                        double &zy, double &zz) {
  double H00 = F_i[0] - 1.0;
  double H01 = F_i[1];
  double H02 = F_i[2];
  double H10 = F_i[3];
  double H11 = F_i[4] - 1.0;
  double H12 = F_i[5];
  double H20 = F_i[6];
  double H21 = F_i[7];
  double H22 = F_i[8] - 1.0;

  double pre_x = H00 * dx + H01 * dy + H02 * dz;
  double pre_y = H10 * dx + H11 * dy + H12 * dz;
  double pre_z = H20 * dx + H21 * dy + H22 * dz;

  zx = du_x - pre_x;
  zy = du_y - pre_y;
  zz = du_z - pre_z;
}

// =========================================================================
// MechanicalSillingStabilizer 实现
// =========================================================================
void MechanicalSillingStabilizer::applyPenalty(PDContext &ctx) {
  auto &fieldManager = ctx.getFieldManager();
  auto &neighborList = ctx.getNeighborList();
  auto &manager = ctx.getParticleManager();
  const size_t numParticles = manager.getTotalParticles();

  std::vector<double> bulkArr = ExtractBulkModulusArray(manager);
  std::vector<double> rhoArr = ExtractDensityArray(manager);

  const double horizon = neighborList.getHorizon();
  const double delta4 = horizon * horizon * horizon * horizon;
  const double coeff_base = 18.0 / (3.141592653589793 * delta4);

  const double *coords = fieldManager.getFieldAs<double>("Coords")->dataPtr();
  const double *volumes = fieldManager.getFieldAs<double>("Volume")->dataPtr();
  const double *dispPtr =
      fieldManager.getFieldAs<double>("Displacement")->dataPtr();
  const double *F_Ptr =
      fieldManager.getFieldAs<double>("DeformationGradient")->dataPtr();
  const double *vvPtr = fieldManager.getFieldAs<double>("VHorizon")->dataPtr();
  const double *omegaPtr = neighborList.getBondFieldPtr("InfluenceWeight");
  double *accPtr = fieldManager.getFieldAs<double>("Acceleration")->dataPtr();

#pragma omp parallel for schedule(static)
  for (int i = 0; i < static_cast<int>(numParticles); ++i) {
    double xi_x = coords[i * 3];
    double xi_y = coords[i * 3 + 1];
    double xi_z = coords[i * 3 + 2];

    double u_ix = dispPtr[i * 3];
    double u_iy = dispPtr[i * 3 + 1];
    double u_iz = dispPtr[i * 3 + 2];

    double vv_i = vvPtr[i];
    double bulk_i = bulkArr[i];
    double G_i = g0_ * bulk_i;
    double coeff_i = coeff_base * G_i / vv_i;
    const double *Fi = &F_Ptr[i * 9];

    double force_x = 0.0, force_y = 0.0, force_z = 0.0;

    const int numNeighbors = neighborList.getNeighborCount(i);
    const int *neighbors = neighborList.getNeighborIds(i);
    const int offset = neighbors - neighborList.getNeighborIds(0);

    for (int k_nb = 0; k_nb < numNeighbors; ++k_nb) {
      int j = neighbors[k_nb];
      if (j == -1)
        continue;

      double dx = coords[j * 3] - xi_x;
      double dy = coords[j * 3 + 1] - xi_y;
      double dz = coords[j * 3 + 2] - xi_z;

      double du_x = dispPtr[j * 3] - u_ix;
      double du_y = dispPtr[j * 3 + 1] - u_iy;
      double du_z = dispPtr[j * 3 + 2] - u_iz;

      double omega = omegaPtr[offset + k_nb];
      double vj = volumes[j];

      double z_ix, z_iy, z_iz;
      ComputeNonAffineDisp(du_x, du_y, du_z, dx, dy, dz, Fi, z_ix, z_iy, z_iz);

      const double *Fj = &F_Ptr[j * 9];
      double z_jx, z_jy, z_jz;
      ComputeNonAffineDisp(-du_x, -du_y, -du_z, -dx, -dy, -dz, Fj, z_jx, z_jy,
                           z_jz);

      double bulk_j = bulkArr[j];
      double G_j = g0_ * bulk_j;
      double vv_j = vvPtr[j];
      double coeff_j = coeff_base * G_j / vv_j;

      force_x += omega * vj / horizon * (coeff_i * z_ix - coeff_j * z_jx);
      force_y += omega * vj / horizon * (coeff_i * z_iy - coeff_j * z_jy);
      force_z += omega * vj / horizon * (coeff_i * z_iz - coeff_j * z_jz);
    }

    if (rhoArr[i] > 0.0) {
      accPtr[i * 3] += force_x / rhoArr[i];
      accPtr[i * 3 + 1] += force_y / rhoArr[i];
      accPtr[i * 3 + 2] += force_z / rhoArr[i];
    }
  }
}

// =========================================================================
// MechanicalWanStabilizer 实现
// =========================================================================
void MechanicalWanStabilizer::applyPenalty(PDContext &ctx) {
  auto &fieldManager = ctx.getFieldManager();
  auto &neighborList = ctx.getNeighborList();
  auto &manager = ctx.getParticleManager();

  const size_t numParticles = manager.getTotalParticles();
  std::vector<double> lambdaArr, muArr;
  ExtractLameParameters(manager, lambdaArr, muArr);
  std::vector<double> rhoArr = ExtractDensityArray(manager);

  const double *coords = fieldManager.getFieldAs<double>("Coords")->dataPtr();
  const double *volumes = fieldManager.getFieldAs<double>("Volume")->dataPtr();
  const double *dispPtr =
      fieldManager.getFieldAs<double>("Displacement")->dataPtr();
  const double *F_Ptr =
      fieldManager.getFieldAs<double>("DeformationGradient")->dataPtr();
  const double *shapeInvPtr =
      fieldManager.getFieldAs<double>("ShapeTensorInv")->dataPtr();
  const double *omegaPtr = neighborList.getBondFieldPtr("InfluenceWeight");
  double *accPtr = fieldManager.getFieldAs<double>("Acceleration")->dataPtr();

#pragma omp parallel for schedule(static)
  for (int i = 0; i < static_cast<int>(numParticles); ++i) {
    double xi_x = coords[i * 3];
    double xi_y = coords[i * 3 + 1];
    double xi_z = coords[i * 3 + 2];

    double u_ix = dispPtr[i * 3];
    double u_iy = dispPtr[i * 3 + 1];
    double u_iz = dispPtr[i * 3 + 2];

    double trace_K_inv_i =
        shapeInvPtr[i * 9] + shapeInvPtr[i * 9 + 4] + shapeInvPtr[i * 9 + 8];
    double lam_i = lambdaArr[i];
    double mu_i = muArr[i];
    double A_i[9];
    for (int r = 0; r < 3; ++r) {
      for (int c = 0; c < 3; ++c) {
        A_i[r * 3 + c] = 2.0 * mu_i * shapeInvPtr[i * 9 + r * 3 + c];
      }
      A_i[r * 3 + r] += lam_i * trace_K_inv_i;
    }
    const double *Fi = &F_Ptr[i * 9];

    double force_x = 0.0, force_y = 0.0, force_z = 0.0;

    const int numNeighbors = neighborList.getNeighborCount(i);
    const int *neighbors = neighborList.getNeighborIds(i);
    const int offset = neighbors - neighborList.getNeighborIds(0);

    for (int k_nb = 0; k_nb < numNeighbors; ++k_nb) {
      int j = neighbors[k_nb];
      if (j == -1)
        continue;

      double dx = coords[j * 3] - xi_x;
      double dy = coords[j * 3 + 1] - xi_y;
      double dz = coords[j * 3 + 2] - xi_z;

      double du_x = dispPtr[j * 3] - u_ix;
      double du_y = dispPtr[j * 3 + 1] - u_iy;
      double du_z = dispPtr[j * 3 + 2] - u_iz;

      double omega = omegaPtr[offset + k_nb];
      double vj = volumes[j];

      double z_ix, z_iy, z_iz;
      ComputeNonAffineDisp(du_x, du_y, du_z, dx, dy, dz, Fi, z_ix, z_iy, z_iz);

      const double *Fj = &F_Ptr[j * 9];
      double z_jx, z_jy, z_jz;
      ComputeNonAffineDisp(-du_x, -du_y, -du_z, -dx, -dy, -dz, Fj, z_jx, z_jy,
                           z_jz);

      double trace_K_inv_j =
          shapeInvPtr[j * 9] + shapeInvPtr[j * 9 + 4] + shapeInvPtr[j * 9 + 8];
      double lam_j = lambdaArr[j];
      double mu_j = muArr[j];
      double A_j[9];
      for (int r = 0; r < 3; ++r) {
        for (int c = 0; c < 3; ++c) {
          A_j[r * 3 + c] = 2.0 * mu_j * shapeInvPtr[j * 9 + r * 3 + c];
        }
        A_j[r * 3 + r] += lam_j * trace_K_inv_j;
      }

      double Ti_x = g0_ * (A_i[0] * z_ix + A_i[1] * z_iy + A_i[2] * z_iz);
      double Ti_y = g0_ * (A_i[3] * z_ix + A_i[4] * z_iy + A_i[5] * z_iz);
      double Ti_z = g0_ * (A_i[6] * z_ix + A_i[7] * z_iy + A_i[8] * z_iz);

      double Tj_x = g0_ * (A_j[0] * z_jx + A_j[1] * z_jy + A_j[2] * z_jz);
      double Tj_y = g0_ * (A_j[3] * z_jx + A_j[4] * z_jy + A_j[5] * z_jz);
      double Tj_z = g0_ * (A_j[6] * z_jx + A_j[7] * z_jy + A_j[8] * z_jz);

      force_x += omega * vj * (Ti_x - Tj_x);
      force_y += omega * vj * (Ti_y - Tj_y);
      force_z += omega * vj * (Ti_z - Tj_z);
    }

    if (rhoArr[i] > 0.0) {
      accPtr[i * 3] += force_x / rhoArr[i];
      accPtr[i * 3 + 1] += force_y / rhoArr[i];
      accPtr[i * 3 + 2] += force_z / rhoArr[i];
    }
  }
}

// =========================================================================
// MechanicalZhangStabilizer 实现
// =========================================================================
void MechanicalZhangStabilizer::applyPenalty(PDContext &ctx) {
  auto &fieldManager = ctx.getFieldManager();
  auto &neighborList = ctx.getNeighborList();
  auto &manager = ctx.getParticleManager();

  const size_t numParticles = manager.getTotalParticles();
  std::vector<double> lambdaArr, muArr;
  ExtractLameParameters(manager, lambdaArr, muArr);
  std::vector<double> rhoArr = ExtractDensityArray(manager);

  const double *coords = fieldManager.getFieldAs<double>("Coords")->dataPtr();
  const double *volumes = fieldManager.getFieldAs<double>("Volume")->dataPtr();
  const double *dispPtr =
      fieldManager.getFieldAs<double>("Displacement")->dataPtr();
  const double *F_Ptr =
      fieldManager.getFieldAs<double>("DeformationGradient")->dataPtr();
  const double *vvPtr = fieldManager.getFieldAs<double>("VHorizon")->dataPtr();
  const double *shapeInvPtr =
      fieldManager.getFieldAs<double>("ShapeTensorInv")->dataPtr();
  const double *omegaPtr = neighborList.getBondFieldPtr("InfluenceWeight");
  double *accPtr = fieldManager.getFieldAs<double>("Acceleration")->dataPtr();

#pragma omp parallel for schedule(static)
  for (int i = 0; i < static_cast<int>(numParticles); ++i) {
    double xi_x = coords[i * 3];
    double xi_y = coords[i * 3 + 1];
    double xi_z = coords[i * 3 + 2];
    double u_ix = dispPtr[i * 3];
    double u_iy = dispPtr[i * 3 + 1];
    double u_iz = dispPtr[i * 3 + 2];

    double vv_i = vvPtr[i];
    double lam_i = lambdaArr[i];
    double mu_i = muArr[i];
    int idx9_i = i * 9;
    double i_k00 = shapeInvPtr[idx9_i], i_k01 = shapeInvPtr[idx9_i + 1],
           i_k02 = shapeInvPtr[idx9_i + 2];
    double i_k10 = shapeInvPtr[idx9_i + 3], i_k11 = shapeInvPtr[idx9_i + 4],
           i_k12 = shapeInvPtr[idx9_i + 5];
    double i_k20 = shapeInvPtr[idx9_i + 6], i_k21 = shapeInvPtr[idx9_i + 7],
           i_k22 = shapeInvPtr[idx9_i + 8];

    const double *Fi = &F_Ptr[idx9_i];
    double force_x = 0.0, force_y = 0.0, force_z = 0.0;

    const int numNeighbors = neighborList.getNeighborCount(i);
    const int *neighbors = neighborList.getNeighborIds(i);
    const int offset = neighbors - neighborList.getNeighborIds(0);

    for (int k_nb = 0; k_nb < numNeighbors; ++k_nb) {
      int j = neighbors[k_nb];
      if (j == -1)
        continue;

      double dx = coords[j * 3] - xi_x;
      double dy = coords[j * 3 + 1] - xi_y;
      double dz = coords[j * 3 + 2] - xi_z;

      double du_x = dispPtr[j * 3] - u_ix;
      double du_y = dispPtr[j * 3 + 1] - u_iy;
      double du_z = dispPtr[j * 3 + 2] - u_iz;

      double vj = volumes[j];
      double omega = omegaPtr[offset + k_nb];

      double z_ix, z_iy, z_iz;
      ComputeNonAffineDisp(du_x, du_y, du_z, dx, dy, dz, Fi, z_ix, z_iy, z_iz);

      const double *Fj = &F_Ptr[j * 9];
      double z_jx, z_jy, z_jz;
      ComputeNonAffineDisp(-du_x, -du_y, -du_z, -dx, -dy, -dz, Fj, z_jx, z_jy,
                           z_jz);

      // p_i = K^{-1}_i * xi
      double px_i = i_k00 * dx + i_k01 * dy + i_k02 * dz;
      double py_i = i_k10 * dx + i_k11 * dy + i_k12 * dz;
      double pz_i = i_k20 * dx + i_k21 * dy + i_k22 * dz;
      double p_dot_p_i = px_i * px_i + py_i * py_i + pz_i * pz_i;
      double p_dot_z_i = px_i * z_ix + py_i * z_iy + pz_i * z_iz;

      double o2 = omega * omega;
      double lam_mu_i = lam_i + mu_i;
      double vv_j = vvPtr[j];
      double t_ix =
          o2 * (lam_mu_i * p_dot_z_i * px_i + mu_i * p_dot_p_i * z_ix) * vv_j;
      double t_iy =
          o2 * (lam_mu_i * p_dot_z_i * py_i + mu_i * p_dot_p_i * z_iy) * vv_j;
      double t_iz =
          o2 * (lam_mu_i * p_dot_z_i * pz_i + mu_i * p_dot_p_i * z_iz) * vv_j;

      // j 的项 (p_j = K^{-1}_j * xi_j, 但是 xi_j = X_i - X_j = -dx)
      int idx9_j = j * 9;
      double j_k00 = shapeInvPtr[idx9_j], j_k01 = shapeInvPtr[idx9_j + 1],
             j_k02 = shapeInvPtr[idx9_j + 2];
      double j_k10 = shapeInvPtr[idx9_j + 3], j_k11 = shapeInvPtr[idx9_j + 4],
             j_k12 = shapeInvPtr[idx9_j + 5];
      double j_k20 = shapeInvPtr[idx9_j + 6], j_k21 = shapeInvPtr[idx9_j + 7],
             j_k22 = shapeInvPtr[idx9_j + 8];

      double px_j = j_k00 * (-dx) + j_k01 * (-dy) + j_k02 * (-dz);
      double py_j = j_k10 * (-dx) + j_k11 * (-dy) + j_k12 * (-dz);
      double pz_j = j_k20 * (-dx) + j_k21 * (-dy) + j_k22 * (-dz);
      double p_dot_p_j = px_j * px_j + py_j * py_j + pz_j * pz_j;
      double p_dot_z_j = px_j * z_jx + py_j * z_jy + pz_j * z_jz;

      double lam_j = lambdaArr[j];
      double mu_j = muArr[j];
      double lam_mu_j = lam_j + mu_j;
      double t_jx =
          o2 * (lam_mu_j * p_dot_z_j * px_j + mu_j * p_dot_p_j * z_jx) * vv_i;
      double t_jy =
          o2 * (lam_mu_j * p_dot_z_j * py_j + mu_j * p_dot_p_j * z_jy) * vv_i;
      double t_jz =
          o2 * (lam_mu_j * p_dot_z_j * pz_j + mu_j * p_dot_p_j * z_jz) * vv_i;

      force_x += g0_ * (t_ix - t_jx) * vj;
      force_y += g0_ * (t_iy - t_jy) * vj;
      force_z += g0_ * (t_iz - t_jz) * vj;
    }

    if (rhoArr[i] > 0.0) {
      accPtr[i * 3] += force_x / rhoArr[i];
      accPtr[i * 3 + 1] += force_y / rhoArr[i];
      accPtr[i * 3 + 2] += force_z / rhoArr[i];
    }
  }
}

} // namespace PDCommon::Kernel
