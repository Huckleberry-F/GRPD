// ============================================================================
// NOSB_Axis.cpp - 轴对称非常规态基近场动力学 (Axisymmetric NOSB-PD)
// ============================================================================

#include "NOSB_Axis.h"
#include "FieldManager.h"
#include "FieldRegistry.h"
#include "FractureModel.h"
#include "KernelRegistry.h"
#include "Logger.h"
#include "MechanicalMaterial.h"
#include "NeighborList.h"
#include "ParticleManager.h"
#include "StabilizerRegistry.h"
#include <algorithm>
#include <cmath>
#include <omp.h>

// ---------------------------------------------------------------------------
// 编译期静态注册：将 NOSB_Axis 注册到 KernelRegistry
// ---------------------------------------------------------------------------
REGISTER_KERNEL(NOSB_Axis, PDCommon::Kernel::NOSB_Axis)

namespace PDCommon::Kernel {

using namespace PDCommon::Core;
using namespace PDCommon::Model;
using namespace PDCommon::Field;
using namespace PDCommon::Material;
using namespace Eigen;

// 平台安全的 π 常量
static constexpr double kPI = 3.14159265358979323846;
static constexpr double kMinRadius = 1.0e-6;

static inline double GetRingVolume(double radius, double area) {
  return 2.0 * kPI * std::max(radius, kMinRadius) * area;
}

void NOSB_Axis::ComputeAxisymmetricState(PDContext &ctx) {
  auto &manager = ctx.getParticleManager();
  auto &neighborList = ctx.getNeighborList();
  auto &fieldManager = ctx.getFieldManager();

  const size_t numParticles = manager.getTotalParticles();

  // 获取核心物理场
  auto *dispField = fieldManager.getFieldAs<double>("Displacement");
  auto *accelField = fieldManager.getFieldAs<double>("Acceleration");
  auto *shapeInvField = fieldManager.getFieldAs<double>("ShapeTensorInv");

  if (!dispField || !accelField || !shapeInvField) {
    LOG_ERROR("[NOSB_Axis] Core fields missing! Check Displacement, "
              "Acceleration or ShapeTensorInv.");
    return;
  }

  // 获取工作场
  auto *defGradField = fieldManager.getFieldAs<double>("DeformationGradient");
  auto *stressField = fieldManager.getFieldAs<double>("PK1Stress");
  if (!defGradField || !stressField) {
    LOG_ERROR("[NOSB_Axis] 'DeformationGradient' or 'PK1Stress' not found!");
    return;
  }

  auto *activeStatusField = fieldManager.getFieldAs<int>("ActiveStatus");
  const int *activeStatusPtr =
      activeStatusField ? activeStatusField->dataPtr() : nullptr;

  // 获取 HPC 指针
  auto *coordsField = fieldManager.getFieldAs<double>("Coords");
  auto *volumeField = fieldManager.getFieldAs<double>("Volume");
  if (!coordsField || !volumeField) {
    LOG_ERROR("[NOSB_Axis] Critical: Coords or Volume field missing.");
    return;
  }
  const double *coords = coordsField->dataPtr();
  const double *volumes = volumeField->dataPtr();
  const double *dispPtr = dispField->dataPtr();
  // 读取局部的环向 K^-1
  const double *shapeInvPtr = ringKinvCache_.data();
  double *FPtr = defGradField->dataPtr();
  double *PK1Ptr = stressField->dataPtr();
  double *accPtr = accelField->dataPtr();

  const double *omegaPtr = neighborList.getBondFieldPtr("InfluenceWeight");
  if (!omegaPtr) {
    LOG_ERROR("[NOSB_Axis] InfluenceWeight bond field missing.");
    return;
  }

  // =======================================================================
  // 步骤 1: 形变梯度 F 重构 & 环向修正 & 本构计算 & 消冗余张量预计算
  // =======================================================================
#pragma omp parallel
  {
#pragma omp for schedule(guided)
    for (int i = 0; i < static_cast<int>(numParticles); ++i) {
      int idx9 = i * 9;

      if (activeStatusPtr && activeStatusPtr[i] == 0) {
        PK1Ptr[idx9] = PK1Ptr[idx9 + 1] = PK1Ptr[idx9 + 2] = 0.0;
        PK1Ptr[idx9 + 3] = PK1Ptr[idx9 + 4] = PK1Ptr[idx9 + 5] = 0.0;
        PK1Ptr[idx9 + 6] = PK1Ptr[idx9 + 7] = PK1Ptr[idx9 + 8] = 0.0;

        pkKinvCache_[idx9] = pkKinvCache_[idx9 + 1] =
            pkKinvCache_[idx9 + 2] = 0.0;
        pkKinvCache_[idx9 + 3] = pkKinvCache_[idx9 + 4] =
            pkKinvCache_[idx9 + 5] = 0.0;
        pkKinvCache_[idx9 + 6] = pkKinvCache_[idx9 + 7] =
            pkKinvCache_[idx9 + 8] = 0.0;
        continue;
      }

      double xi_x = coords[i * 3];
      double xi_y = coords[i * 3 + 1];
      double xi_z = coords[i * 3 + 2];
      double u_ix = dispPtr[i * 3];
      double u_iy = dispPtr[i * 3 + 1];
      double u_iz = dispPtr[i * 3 + 2];

      double m00 = 0.0, m01 = 0.0, m02 = 0.0;
      double m10 = 0.0, m11 = 0.0, m12 = 0.0;
      double m20 = 0.0, m21 = 0.0, m22 = 0.0;

      const int numNeighbors = neighborList.getNeighborCount(i);
      const int *neighbors = neighborList.getNeighborIds(i);
      const int offset = neighbors - neighborList.getNeighborIds(0);

      for (int k = 0; k < numNeighbors; ++k) {
        int j = neighbors[k];
        if (j == -1)
          continue;

        double dx = coords[j * 3] - xi_x;
        double dy = coords[j * 3 + 1] - xi_y;
        double dz = coords[j * 3 + 2] - xi_z;

        double dux = dispPtr[j * 3] - u_ix;
        double duy = dispPtr[j * 3 + 1] - u_iy;
        double duz = dispPtr[j * 3 + 2] - u_iz;

        double omega = omegaPtr[offset + k];
        double vj = GetRingVolume(coords[j * 3], volumes[j]);
        double vol_omega = omega * vj;

        m00 += vol_omega * dux * dx;
        m01 += vol_omega * dux * dy;
        m02 += vol_omega * dux * dz;
        m10 += vol_omega * duy * dx;
        m11 += vol_omega * duy * dy;
        m12 += vol_omega * duy * dz;
        m20 += vol_omega * duz * dx;
        m21 += vol_omega * duz * dy;
        m22 += vol_omega * duz * dz;
      }

      double k00 = shapeInvPtr[idx9], k01 = shapeInvPtr[idx9 + 1],
             k02 = shapeInvPtr[idx9 + 2];
      double k10 = shapeInvPtr[idx9 + 3], k11 = shapeInvPtr[idx9 + 4],
             k12 = shapeInvPtr[idx9 + 5];
      double k20 = shapeInvPtr[idx9 + 6], k21 = shapeInvPtr[idx9 + 7],
             k22 = shapeInvPtr[idx9 + 8];

      double d00 = m00 * k00 + m01 * k10 + m02 * k20;
      double d01 = m00 * k01 + m01 * k11 + m02 * k21;
      double d02 = m00 * k02 + m01 * k12 + m02 * k22;
      double d10 = m10 * k00 + m11 * k10 + m12 * k20;
      double d11 = m10 * k01 + m11 * k11 + m12 * k21;
      double d12 = m10 * k02 + m11 * k12 + m12 * k22;
      double d20 = m20 * k00 + m21 * k10 + m22 * k20;
      double d21 = m20 * k01 + m21 * k11 + m22 * k21;

      FPtr[idx9] = 1.0 + d00;
      FPtr[idx9 + 1] = d01;
      FPtr[idx9 + 2] = d02;
      FPtr[idx9 + 3] = d10;
      FPtr[idx9 + 4] = 1.0 + d11;
      FPtr[idx9 + 5] = d12;
      FPtr[idx9 + 6] = d20;
      FPtr[idx9 + 7] = d21;
      FPtr[idx9 + 8] = 1.0 + u_ix / std::max(xi_x, kMinRadius);

      if (matArrCache_[i]) {
        Eigen::Matrix3d F_mat;
        F_mat << FPtr[idx9], FPtr[idx9 + 1], FPtr[idx9 + 2], FPtr[idx9 + 3],
            FPtr[idx9 + 4], FPtr[idx9 + 5], FPtr[idx9 + 6], FPtr[idx9 + 7],
            FPtr[idx9 + 8];

        int stateMode = 0;
        if (ctx.isStateFrozen()) {
          stateMode = (ctx.getOuterIter() == 0) ? 1 : 2;
        }
        int effectiveId = i;
        Eigen::Matrix3d P_mat =
            matArrCache_[i]->ComputePK1Stress(F_mat, effectiveId, stateMode);

        double p00 = P_mat(0, 0), p01 = P_mat(0, 1), p02 = P_mat(0, 2);
        double p10 = P_mat(1, 0), p11 = P_mat(1, 1), p12 = P_mat(1, 2);
        double p20 = P_mat(2, 0), p21 = P_mat(2, 1), p22 = P_mat(2, 2);

        PK1Ptr[idx9] = p00;
        PK1Ptr[idx9 + 1] = p01;
        PK1Ptr[idx9 + 2] = p02;
        PK1Ptr[idx9 + 3] = p10;
        PK1Ptr[idx9 + 4] = p11;
        PK1Ptr[idx9 + 5] = p12;
        PK1Ptr[idx9 + 6] = p20;
        PK1Ptr[idx9 + 7] = p21;
        PK1Ptr[idx9 + 8] = p22;

        pkKinvCache_[idx9] = p00 * k00 + p01 * k10 + p02 * k20;
        pkKinvCache_[idx9 + 1] = p00 * k01 + p01 * k11 + p02 * k21;
        pkKinvCache_[idx9 + 2] = p00 * k02 + p01 * k12 + p02 * k22;
        pkKinvCache_[idx9 + 3] = p10 * k00 + p11 * k10 + p12 * k20;
        pkKinvCache_[idx9 + 4] = p10 * k01 + p11 * k11 + p12 * k21;
        pkKinvCache_[idx9 + 5] = p10 * k02 + p11 * k12 + p12 * k22;
        pkKinvCache_[idx9 + 6] = p20 * k00 + p21 * k10 + p22 * k20;
        pkKinvCache_[idx9 + 7] = p20 * k01 + p21 * k11 + p22 * k21;
        pkKinvCache_[idx9 + 8] = p20 * k02 + p21 * k12 + p22 * k22;
      } else {
        PK1Ptr[idx9] = PK1Ptr[idx9 + 1] = PK1Ptr[idx9 + 2] = 0.0;
        PK1Ptr[idx9 + 3] = PK1Ptr[idx9 + 4] = PK1Ptr[idx9 + 5] = 0.0;
        PK1Ptr[idx9 + 6] = PK1Ptr[idx9 + 7] = PK1Ptr[idx9 + 8] = 0.0;

        pkKinvCache_[idx9] = pkKinvCache_[idx9 + 1] =
            pkKinvCache_[idx9 + 2] = 0.0;
        pkKinvCache_[idx9 + 3] = pkKinvCache_[idx9 + 4] =
            pkKinvCache_[idx9 + 5] = 0.0;
        pkKinvCache_[idx9 + 6] = pkKinvCache_[idx9 + 7] =
            pkKinvCache_[idx9 + 8] = 0.0;
      }
    }

    // =====================================================================
    // 步骤 2: 非局部力态散度积分 & 环向特化受力修正
    // =====================================================================
#pragma omp for schedule(guided)
    for (int i = 0; i < static_cast<int>(numParticles); ++i) {
      if (rhoArrCache_[i] <= 0.0 ||
          (activeStatusPtr && activeStatusPtr[i] == 0)) {
        continue;
      }

      int idx9_i = i * 9;
      double PKi_00 = pkKinvCache_[idx9_i],
             PKi_01 = pkKinvCache_[idx9_i + 1],
             PKi_02 = pkKinvCache_[idx9_i + 2];
      double PKi_10 = pkKinvCache_[idx9_i + 3],
             PKi_11 = pkKinvCache_[idx9_i + 4],
             PKi_12 = pkKinvCache_[idx9_i + 5];
      double PKi_20 = pkKinvCache_[idx9_i + 6],
             PKi_21 = pkKinvCache_[idx9_i + 7],
             PKi_22 = pkKinvCache_[idx9_i + 8];

      double xi_x = coords[i * 3];
      double xi_y = coords[i * 3 + 1];
      double xi_z = coords[i * 3 + 2];

      double force_x = 0.0;
      double force_y = 0.0;
      double force_z = 0.0;

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

        double omega = omegaPtr[offset + k_nb];
        double vj = GetRingVolume(coords[j * 3], volumes[j]);
        double vol_omega = omega * vj;

        int idx9_j = j * 9;
        double PKj_00 = pkKinvCache_[idx9_j],
               PKj_01 = pkKinvCache_[idx9_j + 1],
               PKj_02 = pkKinvCache_[idx9_j + 2];
        double PKj_10 = pkKinvCache_[idx9_j + 3],
               PKj_11 = pkKinvCache_[idx9_j + 4],
               PKj_12 = pkKinvCache_[idx9_j + 5];
        double PKj_20 = pkKinvCache_[idx9_j + 6],
               PKj_21 = pkKinvCache_[idx9_j + 7],
               PKj_22 = pkKinvCache_[idx9_j + 8];

        double vx = (PKi_00 + PKj_00) * dx + (PKi_01 + PKj_01) * dy +
                    (PKi_02 + PKj_02) * dz;
        double vy = (PKi_10 + PKj_10) * dx + (PKi_11 + PKj_11) * dy +
                    (PKi_12 + PKj_12) * dz;
        double vz = (PKi_20 + PKj_20) * dx + (PKi_21 + PKj_21) * dy +
                    (PKi_22 + PKj_22) * dz;

        force_x += vol_omega * vx;
        force_y += vol_omega * vy;
        force_z += vol_omega * vz;
      }

      double P33 = PK1Ptr[idx9_i + 8];
      force_x -= P33 / std::max(xi_x, kMinRadius);

      accPtr[i * 3] += force_x / rhoArrCache_[i];
      accPtr[i * 3 + 1] += force_y / rhoArrCache_[i];
      accPtr[i * 3 + 2] += force_z / rhoArrCache_[i];
    }
  }

  // =======================================================================
  // 步骤 3: 零能模式修正 (Stabilizer)
  // =======================================================================
  if (stabilizer_) {
    stabilizer_->applyPenalty(ctx);
  }
}

void NOSB_Axis::RecomputeShapeTensorsWithRingVolume(
    PDContext &ctx, bool allowParticleDeactivation) {
  auto &manager = ctx.getParticleManager();
  auto &neighborList = ctx.getNeighborList();
  auto &fieldManager = ctx.getFieldManager();

  const size_t numParticles = manager.getTotalParticles();

  auto &reg = FieldRegistry::getInstance();
  if (!fieldManager.hasField("ShapeTensorInv")) {
    fieldManager.addField(reg.createField("DoubleField", "ShapeTensorInv", 9));
  }
  if (!fieldManager.hasField("VHorizon")) {
    fieldManager.addField(reg.createField("DoubleField", "VHorizon", 1));
  }
  if (!fieldManager.hasField("SumFac")) {
    fieldManager.addField(reg.createField("DoubleField", "SumFac", 1));
  }
  if (!fieldManager.hasField("SumOmegaRaw")) {
    fieldManager.addField(reg.createField("DoubleField", "SumOmegaRaw", 1));
  }

  auto *shapeInvField = fieldManager.getFieldAs<double>("ShapeTensorInv");
  auto *vHorizonField = fieldManager.getFieldAs<double>("VHorizon");
  auto *sumFacField = fieldManager.getFieldAs<double>("SumFac");
  auto *sumOmegaRawField = fieldManager.getFieldAs<double>("SumOmegaRaw");
  auto *coordsField = fieldManager.getFieldAs<double>("Coords");
  auto *volumeField = fieldManager.getFieldAs<double>("Volume");
  auto *bondIntegrityField = fieldManager.getFieldAs<double>("BondIntegrity");
  auto *activeStatusField = fieldManager.getFieldAs<int>("ActiveStatus");

  if (!shapeInvField || !vHorizonField || !sumFacField ||
      !sumOmegaRawField || !coordsField || !volumeField) {
    LOG_ERROR("[NOSB_Axis] Cannot recompute ring-volume shape tensors: "
              "required fields missing.");
    return;
  }

  if (ringKinvCache_.size() != numParticles * 9) {
    ringKinvCache_.assign(numParticles * 9, 0.0);
  }

  shapeInvField->resize(numParticles);
  vHorizonField->resize(numParticles);
  sumFacField->resize(numParticles);
  sumOmegaRawField->resize(numParticles);

  double *shapeInvPtr = shapeInvField->dataPtr();
  double *ringShapeInvPtr = ringKinvCache_.data();
  double *vHorizonPtr = vHorizonField->dataPtr();
  double *sumFacPtr = sumFacField->dataPtr();
  double *sumOmegaRawPtr = sumOmegaRawField->dataPtr();
  const double *coords = coordsField->dataPtr();
  const double *volumes = volumeField->dataPtr();
  double *bondIntegrityPtr =
      bondIntegrityField ? bondIntegrityField->dataPtr() : nullptr;
  int *activeStatusPtr =
      activeStatusField ? activeStatusField->dataPtr() : nullptr;

  neighborList.registerBondField("InfluenceWeight");
  double *omegaPtr = neighborList.getBondFieldPtr("InfluenceWeight");
  const double horizon = neighborList.getHorizon();
  const int dim = ctx.getDimension();
  const double thickness = ctx.getThickness();

  const int minNeighborsRequired = dim + 1;

#pragma omp parallel for schedule(static)
  for (int i = 0; i < static_cast<int>(numParticles); ++i) {
    Eigen::Map<Eigen::Matrix<double, 3, 3, Eigen::RowMajor>> fieldKinvMap(
        &shapeInvPtr[i * 9]);
    Eigen::Map<Eigen::Matrix<double, 3, 3, Eigen::RowMajor>> ringKinvMap(
        &ringShapeInvPtr[i * 9]);

    if (activeStatusPtr && activeStatusPtr[i] == 0) {
      fieldKinvMap = Eigen::Matrix3d::Identity();
      ringKinvMap = Eigen::Matrix3d::Identity();
      sumFacPtr[i] = 0.0;
      vHorizonPtr[i] = 1.0;
      sumOmegaRawPtr[i] = 1.0;
      continue;
    }

    if (bondIntegrityPtr && bondIntegrityPtr[i] >= 0.99) {
      continue;
    }

    const int numNeighbors = neighborList.getNeighborCount(i);
    const int *neighbors = neighborList.getNeighborIds(i);
    const double *bondLens = neighborList.getBondLengths(i);
    const int offset = neighbors - neighborList.getNeighborIds(0);

    int validNeighborCount = 0;
    for (int k = 0; k < numNeighbors; ++k) {
      if (neighbors[k] != -1) {
        ++validNeighborCount;
      }
    }

    Eigen::Vector3d xi(coords[i * 3], coords[i * 3 + 1],
                       coords[i * 3 + 2]);
    Eigen::Matrix3d K = Eigen::Matrix3d::Zero();
    double sum_fac = 0.0;
    double sum_omega_raw = 0.0;

    for (int k = 0; k < numNeighbors; ++k) {
      int j = neighbors[k];
      double omega_raw = GetInfluenceWeight(bondLens[k], horizon, kernelType_);
      double dx_i =
          (dim == 2) ? std::sqrt(volumes[i] / thickness) : std::cbrt(volumes[i]);
      double radij = dx_i * 0.5;
      double fac = GetPartialVolumeFactor(bondLens[k], horizon, radij);
      double omega = omega_raw * fac;
      omegaPtr[offset + k] = omega;

      if (j == -1) {
        continue;
      }

      sum_fac += fac;
      sum_omega_raw += omega_raw;

      Eigen::Vector3d xj(coords[j * 3], coords[j * 3 + 1],
                         coords[j * 3 + 2]);
      Eigen::Vector3d deltaX = xj - xi;
      double vj = GetRingVolume(coords[j * 3], volumes[j]);

      K += omega * (deltaX * deltaX.transpose()) * vj;
    }

    sumFacPtr[i] = sum_fac;
    sumOmegaRawPtr[i] = sum_omega_raw;
    vHorizonPtr[i] =
        (sum_omega_raw > 1.0e-30) ? (sum_fac / sum_omega_raw) : 1.0;

    if (validNeighborCount < minNeighborsRequired) {
      fieldKinvMap = Eigen::Matrix3d::Identity();
      ringKinvMap = Eigen::Matrix3d::Identity();
      sumFacPtr[i] = sum_fac;
      if (sumOmegaRawPtr[i] <= 1.0e-30) {
        sumOmegaRawPtr[i] = 1.0;
      }
      if (allowParticleDeactivation) {
        if (bondIntegrityPtr) {
          bondIntegrityPtr[i] = 1.0;
        }
        if (activeStatusPtr) {
          activeStatusPtr[i] = 0;
        }
      }
      continue;
    }

    const double trace_K = (dim == 2) ? (K(0, 0) + K(1, 1)) : K.trace();
    K += (trace_K * 2.5e-3 + 1.0e-15) * Eigen::Matrix3d::Identity();

    if (dim == 2) {
      K(2, 0) = K(0, 2) = 0.0;
      K(2, 1) = K(1, 2) = 0.0;
      K(2, 2) = 1.0;
    }

    if (std::abs(K.determinant()) < 1.0e-12) {
      fieldKinvMap = Eigen::Matrix3d::Identity();
      ringKinvMap = Eigen::Matrix3d::Identity();
      if (allowParticleDeactivation) {
        if (bondIntegrityPtr) {
          bondIntegrityPtr[i] = 1.0;
        }
        if (activeStatusPtr) {
          activeStatusPtr[i] = 0;
        }
      }
      continue;
    }

    Eigen::Matrix3d K_inv = K.inverse();
    fieldKinvMap = K_inv;
    ringKinvMap = K_inv;
  }
}

void NOSB_Axis::preCompute(PDCommon::Core::PDContext &ctx) {
  auto &fieldManager = ctx.getFieldManager();
  auto &manager = ctx.getParticleManager();
  const size_t numParticles = manager.getTotalParticles();

  matArrCache_.assign(numParticles, nullptr);
  rhoArrCache_.assign(numParticles, 0.0);
  pkKinvCache_.assign(numParticles * 9, 0.0);
  ringKinvCache_.assign(numParticles * 9, 0.0);

  RecomputeShapeTensorsWithRingVolume(ctx, false);

  const auto &particles = manager.getAllParticles();
#pragma omp parallel for schedule(static)
  for (int i = 0; i < static_cast<int>(numParticles); ++i) {
    auto *mat = dynamic_cast<PDCommon::Material::MechanicalMaterial *>(
        particles[i].getMaterial());
    if (mat) {
      matArrCache_[i] = mat;
      rhoArrCache_[i] = mat->getDensity() * massScaleFactor_;
    }
  }

  auto &reg = FieldRegistry::getInstance();
  auto fField = reg.createField("DoubleField", "DeformationGradient", 9);
  auto pField = reg.createField("DoubleField", "PK1Stress", 9);

  fieldManager.addField(std::move(fField));
  fieldManager.addField(std::move(pField));

  auto *defGradField = fieldManager.getFieldAs<double>("DeformationGradient");
  auto *stressField = fieldManager.getFieldAs<double>("PK1Stress");

  defGradField->resize(numParticles);
  stressField->resize(numParticles);
  defGradField->clearToZero();
  stressField->clearToZero();

  double *FPtr = defGradField->dataPtr();
#pragma omp parallel for schedule(static)
  for (int i = 0; i < static_cast<int>(numParticles); ++i) {
    int idx9 = i * 9;
    FPtr[idx9] = 1.0;
    FPtr[idx9 + 1] = 0.0;
    FPtr[idx9 + 2] = 0.0;
    FPtr[idx9 + 3] = 0.0;
    FPtr[idx9 + 4] = 1.0;
    FPtr[idx9 + 5] = 0.0;
    FPtr[idx9 + 6] = 0.0;
    FPtr[idx9 + 7] = 0.0;
    FPtr[idx9 + 8] = 1.0;
  }

  if (!zeroEnergyMethodStr_.empty() && zeroEnergyMethodStr_ != "None") {
    std::string regName = "Mechanical_" + zeroEnergyMethodStr_;
    if (StabilizerRegistry::getInstance().contains(regName)) {
      stabilizer_ = StabilizerRegistry::getInstance().create(regName);
    }
  } else {
    stabilizer_ = nullptr;
  }

  if (stabilizer_) {
    stabilizer_->setG0(zeroEnergyG0_);
    stabilizer_->setMassScaleFactor(massScaleFactor_);
    stabilizer_->setAxisymmetricIntegration(true);
    stabilizer_->preCompute(ctx);
    LOG_INFO("[NOSB_Axis] Instantiated MechanicalStabilizer using strategy: " +
             zeroEnergyMethodStr_);
  }
}

void NOSB_Axis::computeForceState(PDCommon::Core::PDContext &ctx) {
  ComputeAxisymmetricState(ctx);
}

void NOSB_Axis::postCompute(PDCommon::Core::PDContext &ctx) {
  auto &matManager = ctx.getMaterialManager();
  bool fractureEvaluated = false;

  for (const auto &[name, mat] : matManager.getMaterials()) {
    if (mat && mat->getFractureModel()) {
      mat->getFractureModel()->computeFracture(ctx, mat->getMatId());
      fractureEvaluated = true;
    }
  }

  if (fractureEvaluated && dynamicShapeTensor_) {
    RecomputeShapeTensorsWithRingVolume(ctx, true);
  }
}

std::vector<PDKernel::IntegrationTarget>
NOSB_Axis::getIntegrationTargets() const {
  return {{"Displacement", "Velocity", 3}, {"Velocity", "Acceleration", 3}};
}

} // namespace PDCommon::Kernel
