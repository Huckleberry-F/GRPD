// ============================================================================
// NOSB_T.cpp - 热传导非常规态基近场动力学 (Thermal NOSB-PD)
//
// 核心贡献：
//   无状态求解框架，专注于高性能的纯数学/空间积分。所有状态均托管在 PDContext。
//
// HPC 架构优势：
//   1. 极速邻域遍历：基于 1D CSR 扁平格式 (offsets+neighborIds)。
//   2. 极速断键跳过：利用 if (j == -1) continue; 无分支重惩罚跳过断键。
//   3. 内存连续访问：1D 扁平数组 (SoA) + #pragma omp parallel for
//   schedule(static)。
//   4. 全局数据零拷贝：K⁻¹ 通过 9 分量场全局共享，利用 Eigen::Map 零拷贝读写。
// ============================================================================

#include "NOSB_T.h"
#include "FieldManager.h"
#include "Logger.h"
#include "NeighborList.h"
#include "ParticleManager.h"
#include "ThermalMaterial.h"
#include <cmath>
#include <omp.h>

namespace PDCommon::Kernel {

using namespace PDCommon::Core;
using namespace PDCommon::Model;
using namespace PDCommon::Field;
using namespace PDCommon::Material;
using namespace Eigen;

// ---------------------------------------------------------------------------
// 预计算：形状张量的逆 K⁻¹，仅在时间循环外调用一次
// ---------------------------------------------------------------------------
void NOSB_T::ComputeShapeTensors(PDContext &ctx) {
  auto &manager = ctx.getParticleManager();
  auto &neighborList = ctx.getNeighborList();
  auto &fieldManager = ctx.getFieldManager();

  const size_t numParticles = manager.getTotalParticles();

  // ===================================================================
  // 集中注册 NOSB_T 算法框架所需的全部工作场（仅此一次）
  // ===================================================================
  auto *shapeInvField = fieldManager.registerField<double>("ShapeTensorInv", 9);
  auto *tempGradField = fieldManager.registerField<double>("TempGradient", 3);
  auto *heatFluxField = fieldManager.registerField<double>("HeatFluxVec", 3);

  shapeInvField->resize(numParticles);
  tempGradField->resize(numParticles);
  heatFluxField->resize(numParticles);

  shapeInvField->clearToZero();

  // 提取高性能 SoA 裸指针
  double *shapeInvPtr = shapeInvField->dataPtr();
  auto *coordsField = fieldManager.getFieldAs<double>("Coords");
  auto *volumeField = fieldManager.getFieldAs<double>("Volume");
  if (!coordsField || !volumeField) {
    LOG_ERROR(
        "[NOSB_T] Critical: Coords or Volume field missing in FieldManager!");
    return;
  }
  const double *coords = coordsField->dataPtr();
  const double *volumes = volumeField->dataPtr();

  LOG_INFO("[NOSB_T] Computing Shape Tensor Inverse (K^-1)...");

// =======================================================================
// [HPC 优势] OpenMP 静态调度，赋予每个线程连续的粒子内存块，提升缓存命中率
// =======================================================================
#pragma omp parallel for schedule(static)
  for (int i = 0; i < static_cast<int>(numParticles); ++i) {
    Vector3d xi(coords[i * 3], coords[i * 3 + 1], coords[i * 3 + 2]);
    Matrix3d K = Matrix3d::Zero();

    // 提取粒子 i 的 CSR 邻居表边界
    const int numNeighbors = neighborList.getNeighborCount(i);
    const int *neighbors = neighborList.getNeighborIds(i);
    const double *bondLens = neighborList.getBondLengths(i);

    // ===================================================================
    // [HPC 优势] 纯局部邻居遍历，1D 数组顺序访问
    // ===================================================================
    for (int k = 0; k < numNeighbors; ++k) {
      int j = neighbors[k];

      // [HPC 优势] 遇到断键 (-1) 极速跳过
      if (j == -1)
        continue;

      Vector3d xj(coords[j * 3], coords[j * 3 + 1], coords[j * 3 + 2]);
      Vector3d deltaX = xj - xi;

      // 影响函数 omega(xi) = 1.0 / |xi|，其中 |xi| 是初始键长
      double omega = 1.0 / bondLens[k];
      double vj = volumes[j];

      // 积分累加：K_i += omega * (deltaX ⊗ deltaX) * Vj
      K += omega * (deltaX * deltaX.transpose()) * vj;
    }

    // 正则化：防止 2D 粒子（z=0 平面）导致 K 矩阵奇异
    // 对 3D 问题影响可忽略（1e-20 量级）
    K += 1e-20 * Matrix3d::Identity();

    // 求逆计算
    // 备注：如果矩阵不可逆（例如表面孤立粒子或积分域过小），这里可加伪逆判断或惩罚，当前使用纯逆
    Matrix3d K_inv = K.inverse();

    // [HPC 优势] 使用 Eigen::Map 零拷贝直接覆写到底层 double[9] 连续内存中
    Map<Matrix<double, 3, 3, RowMajor>> K_inv_map(&shapeInvPtr[i * 9]);
    K_inv_map = K_inv;
  }

  LOG_INFO(
      "[NOSB_T] All NOSB fields registered and Shape Tensor Inverse computed.");
}

// ---------------------------------------------------------------------------
// 核心三步：Thermal NOSB 态基积分算法（每个时间步调用）
// ---------------------------------------------------------------------------
void NOSB_T::ComputeThermalState(PDContext &ctx) {
  auto &manager = ctx.getParticleManager();
  auto &neighborList = ctx.getNeighborList();
  auto &fieldManager = ctx.getFieldManager();
  auto &matManager = ctx.getMaterialManager();

  const size_t numParticles = manager.getTotalParticles();

  // 1. 获取核心物理场
  auto *tempField = fieldManager.getFieldAs<double>("Temperature");
  auto *rateField = fieldManager.getFieldAs<double>("TempRate");
  auto *shapeInvField = fieldManager.getFieldAs<double>("ShapeTensorInv");

  if (!shapeInvField) {
    LOG_ERROR("[NOSB_T] 'ShapeTensorInv' not found! Please run "
              "ComputeShapeTensors() before solving.");
    return;
  }

  // 2. 获取 NOSB 专属的工作场（已在 ComputeShapeTensors 中统一注册）
  auto *tempGradField = fieldManager.getFieldAs<double>("TempGradient");
  auto *heatFluxField = fieldManager.getFieldAs<double>("HeatFluxVec");

  if (!tempGradField || !heatFluxField) {
    LOG_ERROR("[NOSB_T] 'TempGradient' or 'HeatFluxVec' not found! Please run "
              "ComputeShapeTensors() first.");
    return;
  }

  // 清空当前步的增量(由 ExplicitEuler 负责清除，这里不能清除否则会覆盖由 BC 应用的热流源项)

  // 3. 提取所有高性能 SoA 裸指针
  auto *coordsField = fieldManager.getFieldAs<double>("Coords");
  auto *volumeField = fieldManager.getFieldAs<double>("Volume");
  if (!coordsField || !volumeField) {
    LOG_ERROR(
        "[NOSB_T] Critical: Coords or Volume field missing in FieldManager!");
    return;
  }
  const double *coords = coordsField->dataPtr();
  const double *volumes = volumeField->dataPtr();

  const double *tempPtr = tempField->dataPtr();
  const double *shapeInvPtr = shapeInvField->dataPtr();

  double *gradPtr = tempGradField->dataPtr();
  double *fluxPtr = heatFluxField->dataPtr();
  double *ratePtr = rateField->dataPtr();

  // 用于访问每颗粒子的绑定的具体 ThermalMaterial
  const auto &particles = manager.getAllParticles();

  // ===================================================================
  // [HPC 优化] 预提取材料属性到连续数组，消灭热循环中的 dynamic_cast
  // ===================================================================
  std::vector<double> rhoArr(numParticles), cpArr(numParticles), kArr(numParticles);
  for (size_t i = 0; i < numParticles; ++i) {
      auto* mat = dynamic_cast<ThermalMaterial*>(particles[i].getMaterial());
      if (mat) {
          rhoArr[i] = mat->getDensity();
          cpArr[i]  = mat->getHeatCapacity();
          kArr[i]   = mat->getConductivity();
      }
  }

// =======================================================================
// 步骤 1+2: 非局部温度梯度重构 & 纯局部热流计算 (合并遍历)
// 公式 1: ∇T_i = K_i⁻¹ * [ sum_j ω(ξ) * (T_j - T_i) * ΔX * V_j ]
// 公式 2: q_i = -k * ∇T_i (傅里叶定律)
// =======================================================================
#pragma omp parallel for schedule(static)
  for (int i = 0; i < static_cast<int>(numParticles); ++i) {
    double xi_x = coords[i * 3];
    double xi_y = coords[i * 3 + 1];
    double xi_z = coords[i * 3 + 2];
    double ti = tempPtr[i];

    double sum_x = 0.0, sum_y = 0.0, sum_z = 0.0;

    const int numNeighbors = neighborList.getNeighborCount(i);
    const int *neighbors = neighborList.getNeighborIds(i);
    const double *bondLens = neighborList.getBondLengths(i);

    for (int k = 0; k < numNeighbors; ++k) {
      int j = neighbors[k];
      if (j == -1)
        continue; // 极速跳过断键

      double dx = coords[j * 3] - xi_x;
      double dy = coords[j * 3 + 1] - xi_y;
      double dz = coords[j * 3 + 2] - xi_z;
      
      double deltaT = tempPtr[j] - ti;
      double omega = 1.0 / bondLens[k];
      double vj = volumes[j];

      double factor = omega * deltaT * vj;
      sum_x += factor * dx;
      sum_y += factor * dy;
      sum_z += factor * dz;
    }

    // 从 ShapeInv 数组中提取 3x3 矩阵并展平计算
    int idx9 = i * 9;
    double k00 = shapeInvPtr[idx9],     k01 = shapeInvPtr[idx9 + 1], k02 = shapeInvPtr[idx9 + 2];
    double k10 = shapeInvPtr[idx9 + 3], k11 = shapeInvPtr[idx9 + 4], k12 = shapeInvPtr[idx9 + 5];
    double k20 = shapeInvPtr[idx9 + 6], k21 = shapeInvPtr[idx9 + 7], k22 = shapeInvPtr[idx9 + 8];

    // 计算梯度 (K^-1 * sum)
    double grad_x = k00 * sum_x + k01 * sum_y + k02 * sum_z;
    double grad_y = k10 * sum_x + k11 * sum_y + k12 * sum_z;
    double grad_z = k20 * sum_x + k21 * sum_y + k22 * sum_z;

    gradPtr[i * 3] = grad_x;
    gradPtr[i * 3 + 1] = grad_y;
    gradPtr[i * 3 + 2] = grad_z;

    // --- 紧接着执行类似 Step 2 的操作 ---
    double k_i = kArr[i];
    fluxPtr[i * 3]     = -k_i * grad_x;
    fluxPtr[i * 3 + 1] = -k_i * grad_y;
    fluxPtr[i * 3 + 2] = -k_i * grad_z;
  }

// =======================================================================
// 步骤 3: 非局部热量积分 + 零能模式修正（Hourglass Stabilization）
// =======================================================================
#pragma omp parallel for schedule(static)
  for (int i = 0; i < static_cast<int>(numParticles); ++i) {
    double rho = rhoArr[i];
    double cp = cpArr[i];
    double k = kArr[i];
    if (rho <= 0.0 || cp <= 0.0) continue;
    double thermal_coeff = 1.0 / (rho * cp);

    double G0 = 0.1 * k;

    double xi_x = coords[i * 3];
    double xi_y = coords[i * 3 + 1];
    double xi_z = coords[i * 3 + 2];
    double ti = tempPtr[i];

    double grad_x = gradPtr[i * 3];
    double grad_y = gradPtr[i * 3 + 1];
    double grad_z = gradPtr[i * 3 + 2];

    double qi_x = fluxPtr[i * 3];
    double qi_y = fluxPtr[i * 3 + 1];
    double qi_z = fluxPtr[i * 3 + 2];

    int idx9_i = i * 9;
    double i_k00 = shapeInvPtr[idx9_i],     i_k01 = shapeInvPtr[idx9_i + 1], i_k02 = shapeInvPtr[idx9_i + 2];
    double i_k10 = shapeInvPtr[idx9_i + 3], i_k11 = shapeInvPtr[idx9_i + 4], i_k12 = shapeInvPtr[idx9_i + 5];
    double i_k20 = shapeInvPtr[idx9_i + 6], i_k21 = shapeInvPtr[idx9_i + 7], i_k22 = shapeInvPtr[idx9_i + 8];

    // KQ_i = K_i^-1 * q_i
    double KQi_x = i_k00 * qi_x + i_k01 * qi_y + i_k02 * qi_z;
    double KQi_y = i_k10 * qi_x + i_k11 * qi_y + i_k12 * qi_z;
    double KQi_z = i_k20 * qi_x + i_k21 * qi_y + i_k22 * qi_z;

    double rate_sum_nosb = 0.0;
    double rate_sum_ze = 0.0;

    const int numNeighbors = neighborList.getNeighborCount(i);
    const int *neighbors = neighborList.getNeighborIds(i);
    const double *bondLens = neighborList.getBondLengths(i);

    for (int k_nb = 0; k_nb < numNeighbors; ++k_nb) {
      int j = neighbors[k_nb];
      if (j == -1)
        continue;

      double dx = coords[j * 3] - xi_x;
      double dy = coords[j * 3 + 1] - xi_y;
      double dz = coords[j * 3 + 2] - xi_z;
      
      double omega = 1.0 / bondLens[k_nb];
      double vj = volumes[j];

      // --- NOSB 散度积分 ---
      double qj_x = fluxPtr[j * 3];
      double qj_y = fluxPtr[j * 3 + 1];
      double qj_z = fluxPtr[j * 3 + 2];

      int idx9_j = j * 9;
      double j_k00 = shapeInvPtr[idx9_j],     j_k01 = shapeInvPtr[idx9_j + 1], j_k02 = shapeInvPtr[idx9_j + 2];
      double j_k10 = shapeInvPtr[idx9_j + 3], j_k11 = shapeInvPtr[idx9_j + 4], j_k12 = shapeInvPtr[idx9_j + 5];
      double j_k20 = shapeInvPtr[idx9_j + 6], j_k21 = shapeInvPtr[idx9_j + 7], j_k22 = shapeInvPtr[idx9_j + 8];

      // KQ_j = K_j^-1 * q_j
      double KQj_x = j_k00 * qj_x + j_k01 * qj_y + j_k02 * qj_z;
      double KQj_y = j_k10 * qj_x + j_k11 * qj_y + j_k12 * qj_z;
      double KQj_z = j_k20 * qj_x + j_k21 * qj_y + j_k22 * qj_z;

      // dot product
      double dot_val = (KQi_x + KQj_x) * dx + (KQi_y + KQj_y) * dy + (KQi_z + KQj_z) * dz;
      rate_sum_nosb += omega * dot_val * vj;

      // --- 零能模式修正 ---
      double deltaT_actual = tempPtr[j] - ti;
      double deltaT_predicted = grad_x * dx + grad_y * dy + grad_z * dz;
      double deltaT_res = deltaT_actual - deltaT_predicted;
      double dist2 = dx * dx + dy * dy + dz * dz;

      rate_sum_ze += omega * (deltaT_res / dist2) * vj;
    }

    ratePtr[i] += thermal_coeff * (-rate_sum_nosb + G0 * rate_sum_ze);
  }
}

// ---------------------------------------------------------------------------
// PDKernel 接口实现：委托到内部方法
// ---------------------------------------------------------------------------

void NOSB_T::preCompute(PDCommon::Core::PDContext &ctx) {
  ComputeShapeTensors(ctx);
}

void NOSB_T::computeForceState(PDCommon::Core::PDContext &ctx) {
  ComputeThermalState(ctx);
}

std::vector<PDKernel::IntegrationTarget> NOSB_T::getIntegrationTargets() const {
  return {{"Temperature", "TempRate", 1}};
}

} // namespace PDCommon::Kernel
