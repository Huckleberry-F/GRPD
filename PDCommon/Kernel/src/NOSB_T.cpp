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

  // =======================================================================
  // HPC 核心提速区：合并 OpenMP 并行域，减少屏障与线程调度开销
  // =======================================================================
#pragma omp parallel
  {
    // -----------------------------------------------------------------------
    // 步骤 1+2: 非局部温度梯度重构 & 纯局部热流计算 & 退化本构联乘提取
    // -----------------------------------------------------------------------
#pragma omp for schedule(guided)
    for (int i = 0; i < static_cast<int>(numParticles); ++i) {
      double xi_x = coords[i * 3];
      double xi_y = coords[i * 3 + 1];
      double xi_z = coords[i * 3 + 2];
      double ti = tempPtr[i];

      double sum_x = 0.0, sum_y = 0.0, sum_z = 0.0;

      const int numNeighbors = neighborList.getNeighborCount(i);
      const int *neighbors = neighborList.getNeighborIds(i);
      const int offset = neighbors - neighborList.getNeighborIds(0);

#pragma omp simd
      for (int k = 0; k < numNeighbors; ++k) {
        int j = neighbors[k];
        if (j == -1)
          continue;

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

      // 提取独立材料常数
      double k_i = kArrCache_[i];
      double qx = -k_i * grad_x;
      double qy = -k_i * grad_y;
      double qz = -k_i * grad_z;
      fluxPtr[i * 3] = qx;
      fluxPtr[i * 3 + 1] = qy;
      fluxPtr[i * 3 + 2] = qz;

      // 【HPC 算法降维】极度关键的反张量矢量乘法提取 KQ_i = K_i^-1 * q_i
      kqCache_[i * 3] = k00 * qx + k01 * qy + k02 * qz;
      kqCache_[i * 3 + 1] = k10 * qx + k11 * qy + k12 * qz;
      kqCache_[i * 3 + 2] = k20 * qx + k21 * qy + k22 * qz;
    }

    // -----------------------------------------------------------------------
    // 步骤 3: 非局部热量散度积分
    // -----------------------------------------------------------------------
#pragma omp for schedule(guided)
    for (int i = 0; i < static_cast<int>(numParticles); ++i) {
      double rho = rhoArrCache_[i];
      double cp = cpArrCache_[i];
      if (rho <= 0.0 || cp <= 0.0)
        continue;

      double xi_x = coords[i * 3];
      double xi_y = coords[i * 3 + 1];
      double xi_z = coords[i * 3 + 2];

      // 取出粒子 i 的预计算张量积 (K^-1 * q)_i
      double KQi_x = kqCache_[i * 3];
      double KQi_y = kqCache_[i * 3 + 1];
      double KQi_z = kqCache_[i * 3 + 2];

      double rate_sum_nosb = 0.0;

      const int numNeighbors = neighborList.getNeighborCount(i);
      const int *neighbors = neighborList.getNeighborIds(i);
      const int offset = neighbors - neighborList.getNeighborIds(0);

      // 内围引入 SIMD，消除了一切 3x3 关联
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

        // 极速读取预先计算的 KQ_j
        double KQj_x = kqCache_[j * 3];
        double KQj_y = kqCache_[j * 3 + 1];
        double KQj_z = kqCache_[j * 3 + 2];

        double dot_val =
            (KQi_x + KQj_x) * dx + (KQi_y + KQj_y) * dy + (KQi_z + KQj_z) * dz;
        rate_sum_nosb += omega * dot_val * vj;
      }

      ratePtr[i] -= rate_sum_nosb;
    }
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
    double rho = rhoArrCache_[i];
    double cp = cpArrCache_[i];
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

  // HPC优化：预先分配全部矩阵和材料指针的内存缓存，避免 O(Steps*N) 次堆分配
  matArrCache_.assign(numParticles, nullptr);
  rhoArrCache_.assign(numParticles, 0.0);
  cpArrCache_.assign(numParticles, 0.0);
  kArrCache_.assign(numParticles, 0.0);
  kqCache_.assign(numParticles * 3, 0.0);

  const auto &particles = manager.getAllParticles();
#pragma omp parallel for schedule(static)
  for (int i = 0; i < static_cast<int>(numParticles); ++i) {
    auto *mat = dynamic_cast<PDCommon::Material::ThermalMaterial *>(
        particles[i].getMaterial());
    if (mat) {
      matArrCache_[i] = mat;
      rhoArrCache_[i] = mat->getDensity();
      cpArrCache_[i] = mat->getHeatCapacity();
      kArrCache_[i] = mat->getConductivity();
    }
  }

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
    stabilizer_ = StabilizerRegistry::getInstance().create(regName);
  } else {
    stabilizer_ = nullptr; // 可选的关闭修正
  }

  if (stabilizer_) {
    stabilizer_->setG0(zeroEnergyG0_);
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
