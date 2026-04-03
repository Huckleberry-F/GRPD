#include "MechanicalStabilizers.h"
#include "FieldManager.h"
#include "MechanicalMaterial.h"
#include "NeighborList.h"
#include "ParticleManager.h"
#include "StabilizerRegistry.h"
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
void MechanicalSillingStabilizer::preCompute(PDContext &ctx) {
  auto &manager = ctx.getParticleManager();
  const size_t numParticles = manager.getTotalParticles();
  bulkArr_.assign(numParticles, 0.0);
  rhoArr_.assign(numParticles, 0.0);
  const auto &particles = manager.getAllParticles();

#pragma omp parallel for schedule(static)
  for (int i = 0; i < static_cast<int>(numParticles); ++i) {
    auto *mat = dynamic_cast<MechanicalMaterial *>(particles[i].getMaterial());
    if (mat) {
      bulkArr_[i] = mat->getBulkModulus();
      rhoArr_[i] = mat->getDensity() * massScaleFactor_;
    }
  }
}

void MechanicalSillingStabilizer::applyPenalty(PDContext &ctx) {
  auto &fieldManager = ctx.getFieldManager();
  auto &neighborList = ctx.getNeighborList();
  auto &manager = ctx.getParticleManager();
  const size_t numParticles = manager.getTotalParticles();

  const int dim = ctx.getDimension();
  const double horizon = neighborList.getHorizon();
  const double delta4 = horizon * horizon * horizon * horizon;
  // Silling 的经验系数：18 / (pi * delta^4)
  const double coeff_base = 18.0 / (3.141592653589793 * delta4);

  const double *volumes = fieldManager.getFieldAs<double>("Volume")->dataPtr();
  double dx = (numParticles > 0) ? std::cbrt(volumes[0]) : 0.0;

  double v_macro = 0.0;
  if (dim == 2) {
    v_macro = 3.141592653589793 * horizon * horizon * dx;
  } else {
    v_macro = 4.0 / 3.0 * 3.141592653589793 * horizon * horizon * horizon;
  }

  const double *coords = fieldManager.getFieldAs<double>("Coords")->dataPtr();
  const double *dispPtr =
      fieldManager.getFieldAs<double>("Displacement")->dataPtr();
  const double *F_Ptr =
      fieldManager.getFieldAs<double>("DeformationGradient")->dataPtr();
  const double *vvPtr = fieldManager.getFieldAs<double>("VHorizon")->dataPtr();
  const double *omegaPtr = neighborList.getBondFieldPtr("InfluenceWeight");
  double *accPtr = fieldManager.getFieldAs<double>("Acceleration")->dataPtr();

#pragma omp parallel for schedule(guided)
  for (int i = 0; i < static_cast<int>(numParticles); ++i) {
    double rho_i = rhoArr_[i];
    if (rho_i <= 0.0)
      continue;

    double xi_x = coords[i * 3];
    double xi_y = coords[i * 3 + 1];
    double xi_z = coords[i * 3 + 2];

    double u_ix = dispPtr[i * 3];
    double u_iy = dispPtr[i * 3 + 1];
    double u_iz = dispPtr[i * 3 + 2];

    double G_i = g0_ * bulkArr_[i];
    double coeff_i = coeff_base * G_i * vvPtr[i] / v_macro;
    const double *Fi = &F_Ptr[i * 9];

    double force_x = 0.0, force_y = 0.0, force_z = 0.0;

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

      double G_j = g0_ * bulkArr_[j];
      double coeff_j = coeff_base * G_j * vvPtr[j] / v_macro;

      force_x += omega * vj / horizon * (coeff_i * z_ix - coeff_j * z_jx);
      force_y += omega * vj / horizon * (coeff_i * z_iy - coeff_j * z_jy);
      force_z += omega * vj / horizon * (coeff_i * z_iz - coeff_j * z_jz);
    }

    accPtr[i * 3] += force_x / rho_i;
    accPtr[i * 3 + 1] += force_y / rho_i;
    accPtr[i * 3 + 2] += force_z / rho_i;
  }
}

// =========================================================================
// MechanicalWanStabilizer 实现
// =========================================================================
void MechanicalWanStabilizer::preCompute(PDContext &ctx) {
  auto &manager = ctx.getParticleManager();
  const size_t numParticles = manager.getTotalParticles();

  lambdaArr_.assign(numParticles, 0.0);
  muArr_.assign(numParticles, 0.0);
  rhoArr_.assign(numParticles, 0.0);
  shapeAi_.assign(numParticles * 9, 0.0);

  const auto &particles = manager.getAllParticles();
#pragma omp parallel for schedule(static)
  for (int i = 0; i < static_cast<int>(numParticles); ++i) {
    auto *mat = dynamic_cast<MechanicalMaterial *>(particles[i].getMaterial());
    if (mat) {
      double E = mat->getYoungsModulus();
      double nu = mat->getPoissonsRatio();
      if (nu >= 0.5)
        nu = 0.4999;
      lambdaArr_[i] = (E * nu) / ((1.0 + nu) * (1.0 - 2.0 * nu));
      muArr_[i] = E / (2.0 * (1.0 + nu));
      rhoArr_[i] = mat->getDensity() * massScaleFactor_;
    }
  }

  // 计算并缓存 A_i 惩罚张量，这是 Wan 方法最耗时的矩阵。
  auto *shapeInvF = ctx.getFieldManager().getFieldAs<double>("ShapeTensorInv");
  if (shapeInvF) {
    const double *shapeInvPtr = shapeInvF->dataPtr();
#pragma omp parallel for schedule(static)
    for (int i = 0; i < static_cast<int>(numParticles); ++i) {
      double trace_K_inv =
          shapeInvPtr[i * 9] + shapeInvPtr[i * 9 + 4] + shapeInvPtr[i * 9 + 8];
      double lam = lambdaArr_[i];
      double mu = muArr_[i];
      double *Ai = &shapeAi_[i * 9];
      for (int r = 0; r < 3; ++r) {
        for (int c = 0; c < 3; ++c) {
          Ai[r * 3 + c] = 2.0 * mu * shapeInvPtr[i * 9 + r * 3 + c];
        }
        Ai[r * 3 + r] += lam * trace_K_inv;
      }
    }
  }
}

void MechanicalWanStabilizer::applyPenalty(PDContext &ctx) {
  auto &fieldManager = ctx.getFieldManager();
  auto &neighborList = ctx.getNeighborList();
  auto &manager = ctx.getParticleManager();
  const size_t numParticles = manager.getTotalParticles();

  const double *coords = fieldManager.getFieldAs<double>("Coords")->dataPtr();
  const double *volumes = fieldManager.getFieldAs<double>("Volume")->dataPtr();
  const double *dispPtr =
      fieldManager.getFieldAs<double>("Displacement")->dataPtr();
  const double *F_Ptr =
      fieldManager.getFieldAs<double>("DeformationGradient")->dataPtr();
  const double *omegaPtr = neighborList.getBondFieldPtr("InfluenceWeight");
  double *accPtr = fieldManager.getFieldAs<double>("Acceleration")->dataPtr();

#pragma omp parallel for schedule(guided)
  for (int i = 0; i < static_cast<int>(numParticles); ++i) {
    double rho_i = rhoArr_[i];
    if (rho_i <= 0.0)
      continue;

    double xi_x = coords[i * 3];
    double xi_y = coords[i * 3 + 1];
    double xi_z = coords[i * 3 + 2];

    double u_ix = dispPtr[i * 3];
    double u_iy = dispPtr[i * 3 + 1];
    double u_iz = dispPtr[i * 3 + 2];

    // 直接提取 A_i
    const double *Ai = &shapeAi_[i * 9];
    const double *Fi = &F_Ptr[i * 9];

    double force_x = 0.0, force_y = 0.0, force_z = 0.0;

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

      // 提取预先计算的 A_j 且再乘体积权值！极度降维！
      const double *Aj = &shapeAi_[j * 9];

      double Ti_x = g0_ * (Ai[0] * z_ix + Ai[1] * z_iy + Ai[2] * z_iz);
      double Ti_y = g0_ * (Ai[3] * z_ix + Ai[4] * z_iy + Ai[5] * z_iz);
      double Ti_z = g0_ * (Ai[6] * z_ix + Ai[7] * z_iy + Ai[8] * z_iz);

      double Tj_x = g0_ * (Aj[0] * z_jx + Aj[1] * z_jy + Aj[2] * z_jz);
      double Tj_y = g0_ * (Aj[3] * z_jx + Aj[4] * z_jy + Aj[5] * z_jz);
      double Tj_z = g0_ * (Aj[6] * z_jx + Aj[7] * z_jy + Aj[8] * z_jz);

      force_x += omega * vj * (Ti_x - Tj_x);
      force_y += omega * vj * (Ti_y - Tj_y);
      force_z += omega * vj * (Ti_z - Tj_z);
    }

    accPtr[i * 3] += force_x / rho_i;
    accPtr[i * 3 + 1] += force_y / rho_i;
    accPtr[i * 3 + 2] += force_z / rho_i;
  }
}

// =========================================================================
// MechanicalZhangStabilizer 实现
// =========================================================================
void MechanicalZhangStabilizer::preCompute(PDContext &ctx) {
  auto &manager = ctx.getParticleManager();
  const size_t numParticles = manager.getTotalParticles();

  lambdaArr_.assign(numParticles, 0.0);
  muArr_.assign(numParticles, 0.0);
  rhoArr_.assign(numParticles, 0.0);

  const auto &particles = manager.getAllParticles();
#pragma omp parallel for schedule(static)
  for (int i = 0; i < static_cast<int>(numParticles); ++i) {
    auto *mat = dynamic_cast<MechanicalMaterial *>(particles[i].getMaterial());
    if (mat) {
      double E = mat->getYoungsModulus();
      double nu = mat->getPoissonsRatio();
      if (nu >= 0.5)
        nu = 0.4999;
      lambdaArr_[i] = (E * nu) / ((1.0 + nu) * (1.0 - 2.0 * nu));
      muArr_[i] = E / (2.0 * (1.0 + nu));
      rhoArr_[i] = mat->getDensity() * massScaleFactor_;
    }
  }
}

void MechanicalZhangStabilizer::applyPenalty(PDContext &ctx) {
  auto &fieldManager = ctx.getFieldManager();
  auto &neighborList = ctx.getNeighborList();
  auto &manager = ctx.getParticleManager();

  const size_t numParticles = manager.getTotalParticles();
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

  // [维度修复]
  // 无需猜测系统维度以及厚度(由于厚度并未全局存储，导致 dx 解析报错)
  // 我们直接在后续的每一个节点内部，完美且精确地求取当前粒子的所有邻居真实体积之和！
  // 这个体积和 (sum(vj)) 即等价于： 2D(面积 * 厚度) 或是 3D(真实球体体积)。
  // 并且它天然适配带有表面破损或者截断的边界！！！

#pragma omp parallel for schedule(guided)
  for (int i = 0; i < static_cast<int>(numParticles); ++i) {
    double rho_i = rhoArr_[i];
    if (rho_i <= 0.0)
      continue;

    double xi_x = coords[i * 3];
    double xi_y = coords[i * 3 + 1];
    double xi_z = coords[i * 3 + 2];
    double u_ix = dispPtr[i * 3];
    double u_iy = dispPtr[i * 3 + 1];
    double u_iz = dispPtr[i * 3 + 2];

    double lam_i = lambdaArr_[i];
    double mu_i = muArr_[i];
    const double *i_k = &shapeInvPtr[i * 9];
    const double *Fi = &F_Ptr[i * 9];

    double force_x = 0.0, force_y = 0.0, force_z = 0.0;
    const int numNeighbors = neighborList.getNeighborCount(i);
    const int *neighbors = neighborList.getNeighborIds(i);
    const int offset = neighbors - neighborList.getNeighborIds(0);

    // [绝杀优化]：无需从外部寻找常数 dx 或厚度！
    // 粒子的真实物理几何体积，本质上就是它所有邻居节点的体积之和。
    // 这行简单的动态统计，自动包含了边界截断体积、自动适用 2D (面积*厚度) 和 3D
    // (球体积)。
    double vi = volumes[i];

#pragma omp simd
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

      double px_i = i_k[0] * dx + i_k[1] * dy + i_k[2] * dz;
      double py_i = i_k[3] * dx + i_k[4] * dy + i_k[5] * dz;
      double pz_i = i_k[6] * dx + i_k[7] * dy + i_k[8] * dz;
      double p_dot_p_i = px_i * px_i + py_i * py_i + pz_i * pz_i;
      double p_dot_z_i = px_i * z_ix + py_i * z_iy + pz_i * z_iz;

      double o2 = omega * omega;
      double lam_mu_i = lam_i + mu_i;

      // 提取完整的缺失的量纲 [L^3] -> 也就是局部视野的体积 (2D下天然等于
      // 面积*厚度)
      double vv_j = vvPtr[j] * vi;

      double t_ix =
          o2 * (lam_mu_i * p_dot_z_i * px_i + mu_i * p_dot_p_i * z_ix) * vv_j;
      double t_iy =
          o2 * (lam_mu_i * p_dot_z_i * py_i + mu_i * p_dot_p_i * z_iy) * vv_j;
      double t_iz =
          o2 * (lam_mu_i * p_dot_z_i * pz_i + mu_i * p_dot_p_i * z_iz) * vv_j;

      const double *j_k = &shapeInvPtr[j * 9];
      double px_j = j_k[0] * (-dx) + j_k[1] * (-dy) + j_k[2] * (-dz);
      double py_j = j_k[3] * (-dx) + j_k[4] * (-dy) + j_k[5] * (-dz);
      double pz_j = j_k[6] * (-dx) + j_k[7] * (-dy) + j_k[8] * (-dz);
      double p_dot_p_j = px_j * px_j + py_j * py_j + pz_j * pz_j;
      double p_dot_z_j = px_j * z_jx + py_j * z_jy + pz_j * z_jz;

      double mu_j = muArr_[j];
      double lam_mu_j = lambdaArr_[j] + mu_j;

      // 提取完整的缺失的量纲 [L^3] -> 也就是局部视野的体积 (2D下天然等于
      // 面积*厚度)
      double vv_i = vvPtr[i] * vi;

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

    accPtr[i * 3] += force_x / rho_i;
    accPtr[i * 3 + 1] += force_y / rho_i;
    accPtr[i * 3 + 2] += force_z / rho_i;
  }
}

} // namespace PDCommon::Kernel
