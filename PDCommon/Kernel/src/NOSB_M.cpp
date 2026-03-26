// ============================================================================
// NOSB_M.cpp - 力学非常规态基近场动力学 (Mechanical NOSB-PD)
// ============================================================================

#include "NOSB_M.h"
#include "FieldManager.h"
#include "FieldRegistry.h"
#include "KernelRegistry.h"
#include "Logger.h"
#include "NeighborList.h"
#include "ParticleManager.h"
#include "StabilizerRegistry.h"
#include "MechanicalMaterial.h"
#include <cmath>
#include <omp.h>

// ---------------------------------------------------------------------------
// 编译期静态注册：将 NOSB_M 注册到 KernelRegistry
// ---------------------------------------------------------------------------
REGISTER_KERNEL(NOSB_M, PDCommon::Kernel::NOSB_M)

namespace PDCommon::Kernel {

using namespace PDCommon::Core;
using namespace PDCommon::Model;
using namespace PDCommon::Field;
using namespace PDCommon::Material;
using namespace Eigen;

void NOSB_M::ComputeMechanicalState(PDContext &ctx) {
  auto &manager = ctx.getParticleManager();
  auto &neighborList = ctx.getNeighborList();
  auto &fieldManager = ctx.getFieldManager();
  
  const size_t numParticles = manager.getTotalParticles();

  // 获取核心物理场 (力学核心为 Displacement, Velocity, Acceleration)
  auto *dispField = fieldManager.getFieldAs<double>("Displacement");
  auto *accelField = fieldManager.getFieldAs<double>("Acceleration");
  auto *shapeInvField = fieldManager.getFieldAs<double>("ShapeTensorInv");

  if (!dispField || !accelField || !shapeInvField) {
    LOG_ERROR("[NOSB_M] Core fields missing! Check Displacement, Acceleration or ShapeTensorInv.");
    return;
  }

  // 获取工作场
  auto *defGradField = fieldManager.getFieldAs<double>("DeformationGradient");
  auto *stressField = fieldManager.getFieldAs<double>("PK1Stress");
  if (!defGradField || !stressField) {
    LOG_ERROR("[NOSB_M] 'DeformationGradient' or 'PK1Stress' not found!");
    return;
  }

  // 获取 HPC 指针
  auto *coordsField = fieldManager.getFieldAs<double>("Coords");
  auto *volumeField = fieldManager.getFieldAs<double>("Volume");
  const double *coords = coordsField->dataPtr();
  const double *volumes = volumeField->dataPtr();
  const double *dispPtr = dispField->dataPtr();
  const double *shapeInvPtr = shapeInvField->dataPtr();
  double *FPtr = defGradField->dataPtr();
  double *PPtr = stressField->dataPtr();
  double *accPtr = accelField->dataPtr();

  const double *omegaPtr = neighborList.getBondFieldPtr("InfluenceWeight");
  auto *vHorizonField = fieldManager.getFieldAs<double>("VHorizon");
  const double *vvPtr = vHorizonField->dataPtr();
  const double horizon = neighborList.getHorizon();

  const auto &particles = manager.getAllParticles();

  // HPC 优化：预提取材料属性到数组，避免循环动态转型
  std::vector<MechanicalMaterial*> matArr(numParticles, nullptr);
  std::vector<double> rhoArr(numParticles);
  for (size_t i = 0; i < numParticles; ++i) {
    auto *mat = dynamic_cast<MechanicalMaterial*>(particles[i].getMaterial());
    if (mat) {
      matArr[i] = mat;
      rhoArr[i] = mat->getDensity();
    }
  }

  // =======================================================================
  // 步骤 1+2: 非局部形变梯度张量 F 重构 & 本构应力 P 计算
  // D = [sum ω * (u_j - u_i) \otimes (x_j - x_i) * V_j] * K^-1
  // F = I + D
  // =======================================================================
#pragma omp parallel for schedule(static)
  for (int i = 0; i < static_cast<int>(numParticles); ++i) {
    double xi_x = coords[i * 3], xi_y = coords[i * 3 + 1], xi_z = coords[i * 3 + 2];
    double u_ix = dispPtr[i * 3], u_iy = dispPtr[i * 3 + 1], u_iz = dispPtr[i * 3 + 2];

    double m00 = 0.0, m01 = 0.0, m02 = 0.0;
    double m10 = 0.0, m11 = 0.0, m12 = 0.0;
    double m20 = 0.0, m21 = 0.0, m22 = 0.0;

    const int numNeighbors = neighborList.getNeighborCount(i);
    const int *neighbors = neighborList.getNeighborIds(i);
    const int offset = neighbors - neighborList.getNeighborIds(0);

    for (int k = 0; k < numNeighbors; ++k) {
      int j = neighbors[k];
      if (j == -1) continue;

      double dx = coords[j * 3] - xi_x;
      double dy = coords[j * 3 + 1] - xi_y;
      double dz = coords[j * 3 + 2] - xi_z;

      double dux = dispPtr[j * 3] - u_ix;
      double duy = dispPtr[j * 3 + 1] - u_iy;
      double duz = dispPtr[j * 3 + 2] - u_iz;

      double omega = omegaPtr[offset + k];
      double vj = volumes[j];
      double vol_omega = omega * vj;

      m00 += vol_omega * dux * dx; m01 += vol_omega * dux * dy; m02 += vol_omega * dux * dz;
      m10 += vol_omega * duy * dx; m11 += vol_omega * duy * dy; m12 += vol_omega * duy * dz;
      m20 += vol_omega * duz * dx; m21 += vol_omega * duz * dy; m22 += vol_omega * duz * dz;
    }

    int idx9 = i * 9;
    double k00 = shapeInvPtr[idx9], k01 = shapeInvPtr[idx9 + 1], k02 = shapeInvPtr[idx9 + 2];
    double k10 = shapeInvPtr[idx9 + 3], k11 = shapeInvPtr[idx9 + 4], k12 = shapeInvPtr[idx9 + 5];
    double k20 = shapeInvPtr[idx9 + 6], k21 = shapeInvPtr[idx9 + 7], k22 = shapeInvPtr[idx9 + 8];

    double d00 = m00 * k00 + m01 * k10 + m02 * k20;
    double d01 = m00 * k01 + m01 * k11 + m02 * k21;
    double d02 = m00 * k02 + m01 * k12 + m02 * k22;
    double d10 = m10 * k00 + m11 * k10 + m12 * k20;
    double d11 = m10 * k01 + m11 * k11 + m12 * k21;
    double d12 = m10 * k02 + m11 * k12 + m12 * k22;
    double d20 = m20 * k00 + m21 * k10 + m22 * k20;
    double d21 = m20 * k01 + m21 * k11 + m22 * k21;
    double d22 = m20 * k02 + m21 * k12 + m22 * k22;

    FPtr[idx9]   = 1.0 + d00; FPtr[idx9+1] = d01;       FPtr[idx9+2] = d02;
    FPtr[idx9+3] = d10;       FPtr[idx9+4] = 1.0 + d11; FPtr[idx9+5] = d12;
    FPtr[idx9+6] = d20;       FPtr[idx9+7] = d21;       FPtr[idx9+8] = 1.0 + d22;

    if (matArr[i]) {
      Eigen::Matrix3d F_mat;
      F_mat << FPtr[idx9], FPtr[idx9+1], FPtr[idx9+2],
               FPtr[idx9+3], FPtr[idx9+4], FPtr[idx9+5],
               FPtr[idx9+6], FPtr[idx9+7], FPtr[idx9+8];
      
      Eigen::Matrix3d P_mat = matArr[i]->ComputePK1Stress(F_mat);
      
      PPtr[idx9]   = P_mat(0,0); PPtr[idx9+1] = P_mat(0,1); PPtr[idx9+2] = P_mat(0,2);
      PPtr[idx9+3] = P_mat(1,0); PPtr[idx9+4] = P_mat(1,1); PPtr[idx9+5] = P_mat(1,2);
      PPtr[idx9+6] = P_mat(2,0); PPtr[idx9+7] = P_mat(2,1); PPtr[idx9+8] = P_mat(2,2);
    }
  }

  // =======================================================================
  // 步骤 3: 非局部力态散度积分
  // force_density_i = sum [ω * ( P_i K_i^-1 + P_j K_j^-1 ) * (x_j - x_i)] * V_j
  // =======================================================================
#pragma omp parallel for schedule(static)
  for (int i = 0; i < static_cast<int>(numParticles); ++i) {
    if (rhoArr[i] <= 0.0) continue;

    int idx9_i = i * 9;
    double p00 = PPtr[idx9_i], p01 = PPtr[idx9_i+1], p02 = PPtr[idx9_i+2];
    double p10 = PPtr[idx9_i+3], p11 = PPtr[idx9_i+4], p12 = PPtr[idx9_i+5];
    double p20 = PPtr[idx9_i+6], p21 = PPtr[idx9_i+7], p22 = PPtr[idx9_i+8];

    double i_k00 = shapeInvPtr[idx9_i], i_k01 = shapeInvPtr[idx9_i+1], i_k02 = shapeInvPtr[idx9_i+2];
    double i_k10 = shapeInvPtr[idx9_i+3], i_k11 = shapeInvPtr[idx9_i+4], i_k12 = shapeInvPtr[idx9_i+5];
    double i_k20 = shapeInvPtr[idx9_i+6], i_k21 = shapeInvPtr[idx9_i+7], i_k22 = shapeInvPtr[idx9_i+8];

    double PKi_00 = p00 * i_k00 + p01 * i_k10 + p02 * i_k20;
    double PKi_01 = p00 * i_k01 + p01 * i_k11 + p02 * i_k21;
    double PKi_02 = p00 * i_k02 + p01 * i_k12 + p02 * i_k22;
    double PKi_10 = p10 * i_k00 + p11 * i_k10 + p12 * i_k20;
    double PKi_11 = p10 * i_k01 + p11 * i_k11 + p12 * i_k21;
    double PKi_12 = p10 * i_k02 + p11 * i_k12 + p12 * i_k22;
    double PKi_20 = p20 * i_k00 + p21 * i_k10 + p22 * i_k20;
    double PKi_21 = p20 * i_k01 + p21 * i_k11 + p22 * i_k21;
    double PKi_22 = p20 * i_k02 + p21 * i_k12 + p22 * i_k22;

    double xi_x = coords[i * 3], xi_y = coords[i * 3 + 1], xi_z = coords[i * 3 + 2];
    double force_x = 0.0, force_y = 0.0, force_z = 0.0;

    const int numNeighbors = neighborList.getNeighborCount(i);
    const int *neighbors = neighborList.getNeighborIds(i);
    const int offset = neighbors - neighborList.getNeighborIds(0);

    for (int k_nb = 0; k_nb < numNeighbors; ++k_nb) {
      int j = neighbors[k_nb];
      if (j == -1) continue;

      double dx = coords[j * 3] - xi_x;
      double dy = coords[j * 3 + 1] - xi_y;
      double dz = coords[j * 3 + 2] - xi_z;

      double omega = omegaPtr[offset + k_nb];
      double vj = volumes[j];

      int idx9_j = j * 9;
      double pj00 = PPtr[idx9_j], pj01 = PPtr[idx9_j+1], pj02 = PPtr[idx9_j+2];
      double pj10 = PPtr[idx9_j+3], pj11 = PPtr[idx9_j+4], pj12 = PPtr[idx9_j+5];
      double pj20 = PPtr[idx9_j+6], pj21 = PPtr[idx9_j+7], pj22 = PPtr[idx9_j+8];

      double j_k00 = shapeInvPtr[idx9_j], j_k01 = shapeInvPtr[idx9_j+1], j_k02 = shapeInvPtr[idx9_j+2];
      double j_k10 = shapeInvPtr[idx9_j+3], j_k11 = shapeInvPtr[idx9_j+4], j_k12 = shapeInvPtr[idx9_j+5];
      double j_k20 = shapeInvPtr[idx9_j+6], j_k21 = shapeInvPtr[idx9_j+7], j_k22 = shapeInvPtr[idx9_j+8];

      double PKj_00 = pj00 * j_k00 + pj01 * j_k10 + pj02 * j_k20;
      double PKj_01 = pj00 * j_k01 + pj01 * j_k11 + pj02 * j_k21;
      double PKj_02 = pj00 * j_k02 + pj01 * j_k12 + pj02 * j_k22;
      double PKj_10 = pj10 * j_k00 + pj11 * j_k10 + pj12 * j_k20;
      double PKj_11 = pj10 * j_k01 + pj11 * j_k11 + pj12 * j_k21;
      double PKj_12 = pj10 * j_k02 + pj11 * j_k12 + pj12 * j_k22;
      double PKj_20 = pj20 * j_k00 + pj21 * j_k10 + pj22 * j_k20;
      double PKj_21 = pj20 * j_k01 + pj21 * j_k11 + pj22 * j_k21;
      double PKj_22 = pj20 * j_k02 + pj21 * j_k12 + pj22 * j_k22;

      double vx = (PKi_00 + PKj_00) * dx + (PKi_01 + PKj_01) * dy + (PKi_02 + PKj_02) * dz;
      double vy = (PKi_10 + PKj_10) * dx + (PKi_11 + PKj_11) * dy + (PKi_12 + PKj_12) * dz;
      double vz = (PKi_20 + PKj_20) * dx + (PKi_21 + PKj_21) * dy + (PKi_22 + PKj_22) * dz;

      force_x += omega * vx * vj;
      force_y += omega * vy * vj;
      force_z += omega * vz * vj;
    }

    // 更新加速度 (等效体积力 / 质量密度)
    // 假设未受其他显式外力约束时直接覆盖当前时刻加速度；如使用力场需先归零。
    // 在这个框架里，积分器先从力场累加力，我们在这里将其加到现有加速度上。
    accPtr[i * 3]     += force_x / rhoArr[i];
    accPtr[i * 3 + 1] += force_y / rhoArr[i];
    accPtr[i * 3 + 2] += force_z / rhoArr[i];
  }

  // 4. 应用零能模式修正 (若有)
  if (stabilizer_) {
    stabilizer_->applyPenalty(ctx);
  }
}

void NOSB_M::preCompute(PDCommon::Core::PDContext &ctx) {
  ComputeShapeTensors(ctx);

  auto &fieldManager = ctx.getFieldManager();
  auto &manager = ctx.getParticleManager();
  const size_t numParticles = manager.getTotalParticles();

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
    FPtr[idx9] = 1.0; FPtr[idx9+1] = 0.0; FPtr[idx9+2] = 0.0;
    FPtr[idx9+3] = 0.0; FPtr[idx9+4] = 1.0; FPtr[idx9+5] = 0.0;
    FPtr[idx9+6] = 0.0; FPtr[idx9+7] = 0.0; FPtr[idx9+8] = 1.0;
  }

  if (!zeroEnergyMethodStr_.empty() && zeroEnergyMethodStr_ != "None") {
    std::string regName = "Mechanical_" + zeroEnergyMethodStr_;
    if (StabilizerRegistry::getInstance().hasType(regName)) {
      stabilizer_ = StabilizerRegistry::getInstance().create(regName);
      stabilizer_->setG0(zeroEnergyG0_);
      stabilizer_->preCompute(ctx);
    }
  } else {
    stabilizer_ = nullptr;
  }
}

void NOSB_M::computeForceState(PDCommon::Core::PDContext &ctx) {
  ComputeMechanicalState(ctx);
}

std::vector<PDKernel::IntegrationTarget> NOSB_M::getIntegrationTargets() const {
  // 定义 L1(积分器)-L2(空间内核) 的拓扑对接链
  // 对二阶中心差分系统：
  // Target 1: Displacement -> Velocity
  // Target 2: Velocity -> Acceleration
  return {
    {"Displacement", "Velocity", 3},
    {"Velocity", "Acceleration", 3}
  };
}

} // namespace PDCommon::Kernel
