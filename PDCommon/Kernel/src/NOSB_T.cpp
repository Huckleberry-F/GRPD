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
#include "FieldRegistry.h"
#include "KernelRegistry.h"
#include "Logger.h"
#include "NeighborList.h"
#include "ParticleManager.h"
#include "StabilizerRegistry.h"
#include "ThermalMaterial.h"
#include <cmath>
#include <omp.h>


// ---------------------------------------------------------------------------
// 编译期静态注册：将 NOSB_T 以 "NOSB_Thermal" 名称注入 KernelRegistry
// ---------------------------------------------------------------------------
REGISTER_KERNEL(NOSB_T, PDCommon::Kernel::NOSB_T)

namespace PDCommon::Kernel {

using namespace PDCommon::Core;
using namespace PDCommon::Model;
using namespace PDCommon::Field;
using namespace PDCommon::Material;
using namespace Eigen;

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

  // 2. 获取 NOSB 专属的工作场
  auto *tempGradField = fieldManager.getFieldAs<double>("TempGradient");
  auto *heatFluxField = fieldManager.getFieldAs<double>("HeatFluxVec");
  if (!tempGradField || !heatFluxField) {
    LOG_ERROR("[NOSB_T] 'TempGradient' or 'HeatFluxVec' not found! Please run "
              "ComputeShapeTensors() first.");
    return;
  }

  // 清空当前步的增量(由 ExplicitEuler 负责清除，这里不能清除否则会覆盖由 BC
  // 应用的热流源项)

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

  // 获取预计算的影响函数权重 BondField
  const double *omegaPtr = neighborList.getBondFieldPtr("InfluenceWeight");

  // 获取预计算的部分体积因子比例
  auto *vHorizonField = fieldManager.getFieldAs<double>("VHorizon");
  const double *vvPtr = vHorizonField->dataPtr();
  const double horizon = neighborList.getHorizon();

  // 用于访问每颗粒子的绑定的具体 ThermalMaterial
  const auto &particles = manager.getAllParticles();

  // ===================================================================
  // [HPC 优化] 预提取材料属性到连续数组，消灭热循环中的 dynamic_cast
  // ===================================================================
  std::vector<double> rhoArr(numParticles), cpArr(numParticles),
      kArr(numParticles);
  for (size_t i = 0; i < numParticles; ++i) {
    auto *mat = dynamic_cast<ThermalMaterial *>(particles[i].getMaterial());
    if (mat) {
      rhoArr[i] = mat->getDensity();
      cpArr[i] = mat->getHeatCapacity();
      kArr[i] = mat->getConductivity();
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
    const int offset = neighbors - neighborList.getNeighborIds(0);

    for (int k = 0; k < numNeighbors; ++k) {
      int j = neighbors[k];
      if (j == -1)
        continue; // 极速跳过断键

      double dx = coords[j * 3] - xi_x;
      double dy = coords[j * 3 + 1] - xi_y;
      double dz = coords[j * 3 + 2] - xi_z;

      double deltaT = tempPtr[j] - ti;
      double omega = omegaPtr[offset + k];
      double vj = volumes[j];

      double factor = omega * deltaT * vj;
      sum_x += factor * dx;
      sum_y += factor * dy;
      sum_z += factor * dz;
    }

    // 从 ShapeInv 数组中提取 3x3 矩阵并展平计算
    int idx9 = i * 9;
    double k00 = shapeInvPtr[idx9], k01 = shapeInvPtr[idx9 + 1],
           k02 = shapeInvPtr[idx9 + 2];
    double k10 = shapeInvPtr[idx9 + 3], k11 = shapeInvPtr[idx9 + 4],
           k12 = shapeInvPtr[idx9 + 5];
    double k20 = shapeInvPtr[idx9 + 6], k21 = shapeInvPtr[idx9 + 7],
           k22 = shapeInvPtr[idx9 + 8];

    // 计算梯度 (K^-1 * sum)
    double grad_x = k00 * sum_x + k01 * sum_y + k02 * sum_z;
    double grad_y = k10 * sum_x + k11 * sum_y + k12 * sum_z;
    double grad_z = k20 * sum_x + k21 * sum_y + k22 * sum_z;

    gradPtr[i * 3] = grad_x;
    gradPtr[i * 3 + 1] = grad_y;
    gradPtr[i * 3 + 2] = grad_z;

    // --- 紧接着执行类似 Step 2 的操作 ---
    double k_i = kArr[i];
    fluxPtr[i * 3] = -k_i * grad_x;
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
    if (rho <= 0.0 || cp <= 0.0)
      continue;
    double thermal_coeff = 1.0 / (rho * cp);

    double vv_i = vvPtr[i];

    int idx9_i = i * 9;
    double i_k00 = shapeInvPtr[idx9_i], i_k01 = shapeInvPtr[idx9_i + 1],
           i_k02 = shapeInvPtr[idx9_i + 2];
    double i_k10 = shapeInvPtr[idx9_i + 3], i_k11 = shapeInvPtr[idx9_i + 4],
           i_k12 = shapeInvPtr[idx9_i + 5];
    double i_k20 = shapeInvPtr[idx9_i + 6], i_k21 = shapeInvPtr[idx9_i + 7],
           i_k22 = shapeInvPtr[idx9_i + 8];

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

    // 已经在前面提取过了 i_k00 ... i_k22

    // KQ_i = K_i^-1 * q_i
    double KQi_x = i_k00 * qi_x + i_k01 * qi_y + i_k02 * qi_z;
    double KQi_y = i_k10 * qi_x + i_k11 * qi_y + i_k12 * qi_z;
    double KQi_z = i_k20 * qi_x + i_k21 * qi_y + i_k22 * qi_z;

    double rate_sum_nosb = 0.0;
    double rate_sum_ze = 0.0;

    const int numNeighbors = neighborList.getNeighborCount(i);
    const int *neighbors = neighborList.getNeighborIds(i);
    const double *bondLens = neighborList.getBondLengths(i);
    const int offset = neighbors - neighborList.getNeighborIds(0);

    for (int k_nb = 0; k_nb < numNeighbors; ++k_nb) {
      int j = neighbors[k_nb];
      if (j == -1)
        continue;

      double dx = coords[j * 3] - xi_x;
      double dy = coords[j * 3 + 1] - xi_y;
      double dz = coords[j * 3 + 2] - xi_z;

      double omega = omegaPtr[offset + k_nb];
      double vj = volumes[j];

      // --- NOSB 散度积分 ---
      double qj_x = fluxPtr[j * 3];
      double qj_y = fluxPtr[j * 3 + 1];
      double qj_z = fluxPtr[j * 3 + 2];

      int idx9_j = j * 9;
      double j_k00 = shapeInvPtr[idx9_j], j_k01 = shapeInvPtr[idx9_j + 1],
             j_k02 = shapeInvPtr[idx9_j + 2];
      double j_k10 = shapeInvPtr[idx9_j + 3], j_k11 = shapeInvPtr[idx9_j + 4],
             j_k12 = shapeInvPtr[idx9_j + 5];
      double j_k20 = shapeInvPtr[idx9_j + 6], j_k21 = shapeInvPtr[idx9_j + 7],
             j_k22 = shapeInvPtr[idx9_j + 8];

      // KQ_j = K_j^-1 * q_j
      double KQj_x = j_k00 * qj_x + j_k01 * qj_y + j_k02 * qj_z;
      double KQj_y = j_k10 * qj_x + j_k11 * qj_y + j_k12 * qj_z;
      double KQj_z = j_k20 * qj_x + j_k21 * qj_y + j_k22 * qj_z;

      // dot product
      double dot_val =
          (KQi_x + KQj_x) * dx + (KQi_y + KQj_y) * dy + (KQi_z + KQj_z) * dz;
      rate_sum_nosb += omega * dot_val * vj;
    }

    // 累加非局部截断热流散度项（量纲为 W/m^3，进入内部的为正，此处公式 q_nosb
    // 表示外部，所以减去）
    ratePtr[i] -= rate_sum_nosb;
  }

  // 4. 应用独立解耦的零能模式数值修正！
  // 4. 应用独立解耦的零能模式数值修正
  if (stabilizer_) {
    // 稳定器内部会将零能修正热功率 (W/m^3) 直接 += 累加进 ratePtr[i] 中
    stabilizer_->applyPenalty(ctx);
  }

// 5. 最终统一转化量纲：将总热功率密度转化为温度的时间变化率 dT/dt = Q_total /
// (rho * cp)
#pragma omp parallel for schedule(static)
  for (int i = 0; i < static_cast<int>(numParticles); ++i) {
    double rho = rhoArr[i];
    double cp = cpArr[i];
    if (rho > 1e-12 && cp > 1e-12) {
      ratePtr[i] /= (rho * cp);
    }
  }
}

// ---------------------------------------------------------------------------
// PDKernel 接口实现：委托到内部方法
// ---------------------------------------------------------------------------

void NOSB_T::preCompute(PDCommon::Core::PDContext &ctx) {
  // 1. 获取通用形状张量
  ComputeShapeTensors(ctx);

  // 2. 注册热传导特有的本地状态场
  auto &fieldManager = ctx.getFieldManager();
  auto &manager = ctx.getParticleManager();
  const size_t numParticles = manager.getTotalParticles();

  auto &reg = FieldRegistry::getInstance();
  auto gradField = reg.createField("DoubleField", "TempGradient", 3);
  auto fluxField = reg.createField("DoubleField", "HeatFluxVec", 3);
  fieldManager.addField(std::move(gradField));
  fieldManager.addField(std::move(fluxField));
  auto *tempGradField = fieldManager.getFieldAs<double>("TempGradient");
  auto *heatFluxField = fieldManager.getFieldAs<double>("HeatFluxVec");

  if (!tempGradField || !heatFluxField) {
    LOG_ERROR("[NOSB_T] Failed to create TempGradient or HeatFluxVec fields.");
    return;
  }

  tempGradField->resize(numParticles);
  heatFluxField->resize(numParticles);
  tempGradField->clearToZero();
  heatFluxField->clearToZero();

  // 3. 实例化零能模式策略工厂：根据 zeroEnergyMethodStr_ 创建多态稳定化对象
  if (!zeroEnergyMethodStr_.empty() && zeroEnergyMethodStr_ != "None") {
    // 根据老配置映射：如果填"Zhang"，映射为 "Thermal_Zhang"
    std::string regName = "Thermal_" + zeroEnergyMethodStr_;
    stabilizer_ =
        PDCommon::Kernel::StabilizerRegistry::getInstance().create(regName);
  } else {
    stabilizer_ = nullptr; // 可选的关闭修正
  }

  if (stabilizer_) {
    stabilizer_->setG0(zeroEnergyG0_);

    // [工厂延伸] 对于类似 Zhang
    // 方法，要求初始化阶预计算罚函数刚度张量，多态执行：
    stabilizer_->preCompute(ctx);

    LOG_INFO("[NOSB_T] Instantiated ThermalStabilizer globally in Phase 0 "
             "using strategy: " +
             zeroEnergyMethodStr_);
  }
}

void NOSB_T::computeForceState(PDCommon::Core::PDContext &ctx) {
  ComputeThermalState(ctx);
}

std::vector<PDKernel::IntegrationTarget> NOSB_T::getIntegrationTargets() const {
  return {{"Temperature", "TempRate", 1}};
}

} // namespace PDCommon::Kernel
