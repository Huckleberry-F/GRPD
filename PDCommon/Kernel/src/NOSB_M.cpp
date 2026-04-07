// ============================================================================
// NOSB_M.cpp - 力学非常规态基近场动力学 (Mechanical NOSB-PD)
// ============================================================================

#include "NOSB_M.h"
#include "DamageModel.h"
#include "FieldManager.h"
#include "FieldRegistry.h"
#include "KernelRegistry.h"
#include "Logger.h"
#include "MechanicalMaterial.h"
#include "NeighborList.h"
#include "ParticleManager.h"
#include "StabilizerRegistry.h"
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
    LOG_ERROR("[NOSB_M] Core fields missing! Check Displacement, Acceleration "
              "or ShapeTensorInv.");
    return;
  }

  // 获取工作场
  auto *defGradField = fieldManager.getFieldAs<double>("DeformationGradient");
  auto *stressField = fieldManager.getFieldAs<double>("PK1Stress");
  if (!defGradField || !stressField) {
    LOG_ERROR("[NOSB_M] 'DeformationGradient' or 'PK1Stress' not found!");
    return;
  }

  // 尝试获取 Damage 场（可能未启用损伤模型）
  auto *damageField = fieldManager.getFieldAs<double>("Damage");
  const double *damagePtr = damageField ? damageField->dataPtr() : nullptr;

  // 获取 HPC 指针
  auto *coordsField = fieldManager.getFieldAs<double>("Coords");
  auto *volumeField = fieldManager.getFieldAs<double>("Volume");
  const double *coords = coordsField->dataPtr();
  const double *volumes = volumeField->dataPtr();
  const double *dispPtr = dispField->dataPtr();
  const double *shapeInvPtr = shapeInvField->dataPtr();
  double *FPtr = defGradField->dataPtr();
  double *PK1Ptr = stressField->dataPtr();
  double *accPtr = accelField->dataPtr();

  const double *omegaPtr = neighborList.getBondFieldPtr("InfluenceWeight");
  auto *vHorizonField = fieldManager.getFieldAs<double>("VHorizon");
  const double *vvPtr = vHorizonField->dataPtr();
  const double horizon = neighborList.getHorizon();

  // =======================================================================
  // HPC 核心提速区：合并 OpenMP 并行域，减少屏障与线程调度开销
  // =======================================================================
#pragma omp parallel
  {
    // -----------------------------------------------------------------------
    // 步骤 1+2: 非局部形变梯度张量 F 重构 & 本构计算 & 消冗余张量预计算
    // -----------------------------------------------------------------------
#pragma omp for schedule(guided)
    for (int i = 0; i < static_cast<int>(numParticles); ++i) {
      if (damagePtr && damagePtr[i] >= 0.99) {
        int idx9 = i * 9;
        FPtr[idx9] = FPtr[idx9 + 4] = FPtr[idx9 + 8] = 1.0;
        FPtr[idx9 + 1] = FPtr[idx9 + 2] = FPtr[idx9 + 3] = 0.0;
        FPtr[idx9 + 5] = FPtr[idx9 + 6] = FPtr[idx9 + 7] = 0.0;
        
        PK1Ptr[idx9] = PK1Ptr[idx9 + 1] = PK1Ptr[idx9 + 2] = 0.0;
        PK1Ptr[idx9 + 3] = PK1Ptr[idx9 + 4] = PK1Ptr[idx9 + 5] = 0.0;
        PK1Ptr[idx9 + 6] = PK1Ptr[idx9 + 7] = PK1Ptr[idx9 + 8] = 0.0;
        
        pkKinvCache_[idx9] = pkKinvCache_[idx9 + 1] = pkKinvCache_[idx9 + 2] = 0.0;
        pkKinvCache_[idx9 + 3] = pkKinvCache_[idx9 + 4] = pkKinvCache_[idx9 + 5] = 0.0;
        pkKinvCache_[idx9 + 6] = pkKinvCache_[idx9 + 7] = pkKinvCache_[idx9 + 8] = 0.0;
        continue;
      }

      double xi_x = coords[i * 3], xi_y = coords[i * 3 + 1],
             xi_z = coords[i * 3 + 2];
      double u_ix = dispPtr[i * 3], u_iy = dispPtr[i * 3 + 1],
             u_iz = dispPtr[i * 3 + 2];

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
        double vj = volumes[j];
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

      int idx9 = i * 9;
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
      double d22 = m20 * k02 + m21 * k12 + m22 * k22;

      FPtr[idx9] = 1.0 + d00;
      FPtr[idx9 + 1] = d01;
      FPtr[idx9 + 2] = d02;
      FPtr[idx9 + 3] = d10;
      FPtr[idx9 + 4] = 1.0 + d11;
      FPtr[idx9 + 5] = d12;
      FPtr[idx9 + 6] = d20;
      FPtr[idx9 + 7] = d21;
      FPtr[idx9 + 8] = 1.0 + d22;

      if (matArrCache_[i]) {
        Eigen::Matrix3d F_mat;
        F_mat << FPtr[idx9], FPtr[idx9 + 1], FPtr[idx9 + 2], FPtr[idx9 + 3],
            FPtr[idx9 + 4], FPtr[idx9 + 5], FPtr[idx9 + 6], FPtr[idx9 + 7],
            FPtr[idx9 + 8];

        Eigen::Matrix3d P_mat = matArrCache_[i]->ComputePK1Stress(F_mat, i);

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

        // 【算法降维的核心】提前在颗粒 i 的一维循环中计算好有效矩阵 S_i = P_i *
        // K_i^-1
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
        pkKinvCache_[idx9] = pkKinvCache_[idx9 + 1] = pkKinvCache_[idx9 + 2] =
            0.0;
        pkKinvCache_[idx9 + 3] = pkKinvCache_[idx9 + 4] =
            pkKinvCache_[idx9 + 5] = 0.0;
        pkKinvCache_[idx9 + 6] = pkKinvCache_[idx9 + 7] =
            pkKinvCache_[idx9 + 8] = 0.0;
      }
    }

    // -----------------------------------------------------------------------
    // 步骤 3: 非局部力态散度积分
    // 采用 schedule(guided) 平衡边界缺边带来的负载不平均
    // -----------------------------------------------------------------------
#pragma omp for schedule(guided)
    for (int i = 0; i < static_cast<int>(numParticles); ++i) {
      if (rhoArrCache_[i] <= 0.0)
        continue;

      int idx9_i = i * 9;
      // 取出粒子 i 的预计算张量 S_i
      double PKi_00 = pkKinvCache_[idx9_i], PKi_01 = pkKinvCache_[idx9_i + 1],
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

      // 引入 SIMD 指令进行内部矢量化纯加乘计算
#pragma omp simd
      for (int k_nb = 0; k_nb < numNeighbors; ++k_nb) {
        int j = neighbors[k_nb];
        if (j == -1)
          continue;

        double dx = coords[j * 3] - xi_x;
        double dy = coords[j * 3 + 1] - xi_y;
        double dz = coords[j * 3 + 2] - xi_z;

        double omega = omegaPtr[offset + k_nb];
        double vj = volumes[j];
        double vol_omega = omega * vj;

        int idx9_j = j * 9;
        // 取出为粒子 j 预先计算好的张量 S_j！在此内层完全消去矩阵外积操作！
        double PKj_00 = pkKinvCache_[idx9_j], PKj_01 = pkKinvCache_[idx9_j + 1],
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

      accPtr[i * 3] += force_x / rhoArrCache_[i];
      accPtr[i * 3 + 1] += force_y / rhoArrCache_[i];
      accPtr[i * 3 + 2] += force_z / rhoArrCache_[i];
    }
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

  // HPC优化：预先分配全部矩阵和材料指针的内存缓存，避免 O(Steps*N) 次堆调用
  matArrCache_.assign(numParticles, nullptr);
  rhoArrCache_.assign(numParticles, 0.0);
  pkKinvCache_.assign(numParticles * 9, 0.0);

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
    stabilizer_->preCompute(ctx);
    LOG_INFO("[NOSB_M] Instantiated MechanicalStabilizer globally in Phase 0 "
             "using strategy: " +
             zeroEnergyMethodStr_);
  }
  // Damage 相关的解析已迁移至 Material 层
}
void NOSB_M::computeForceState(PDCommon::Core::PDContext &ctx) {
  ComputeMechanicalState(ctx);
}

void NOSB_M::postCompute(PDCommon::Core::PDContext &ctx) {
  auto &matManager = ctx.getMaterialManager();
  bool damageEvaluated = false;
  // 遍历所有注册在系统中的材料
  for (const auto &[name, mat] : matManager.getMaterials()) {
    if (mat && mat->getDamageModel()) {
      // 每个具体的断裂准则模型只会遍历自身对应的粒子
      mat->getDamageModel()->computeDamage(ctx, mat->getMatId());
      damageEvaluated = true;
    }
  }
  
  if (damageEvaluated) {
    // 基于最新存活的键，实时刷新 K 矩阵并彻底摘除坏死点
    UpdateShapeTensors(ctx);
  }
}

std::vector<PDKernel::IntegrationTarget> NOSB_M::getIntegrationTargets() const {
  // 定义 L1(积分器)-L2(空间内核) 的拓扑对接链
  // 对二阶中心差分系统：
  // Target 1: Displacement -> Velocity
  // Target 2: Velocity -> Acceleration
  return {{"Displacement", "Velocity", 3}, {"Velocity", "Acceleration", 3}};
}

} // namespace PDCommon::Kernel
