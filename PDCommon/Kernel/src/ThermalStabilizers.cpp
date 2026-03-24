#include "ThermalStabilizers.h"
#include "FieldManager.h"
#include "FieldRegistry.h"
#include "ParticleManager.h"
#include "NeighborList.h"
#include "ThermalMaterial.h"
#include "Logger.h"
#include <omp.h>
#include <cmath>

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
// 抽取通用缓存数组辅助函数 (提取热传导率)
// =========================================================================
static std::vector<double> ExtractConductivityArray(ParticleManager &manager) {
  const size_t numParticles = manager.getTotalParticles();
  std::vector<double> kArr(numParticles, 0.0);
  const auto &particles = manager.getAllParticles();
#pragma omp parallel for schedule(static)
  for (int i = 0; i < static_cast<int>(numParticles); ++i) {
    auto *mat = dynamic_cast<ThermalMaterial *>(particles[i].getMaterial());
    if (mat) kArr[i] = mat->getConductivity();
  }
  return kArr;
}

// =========================================================================
// SillingStabilizer 实现
// =========================================================================
void SillingStabilizer::applyPenalty(PDContext &ctx) {
  auto &fieldManager = ctx.getFieldManager();
  auto &neighborList = ctx.getNeighborList();
  auto &manager = ctx.getParticleManager();
  const size_t numParticles = manager.getTotalParticles();
  
  // 提取本构热传导系数 kArr
  std::vector<double> kArr = ExtractConductivityArray(manager);
  const double horizon = neighborList.getHorizon();
  const double delta4 = horizon * horizon * horizon * horizon;
  const double coeff_base = 6.0 / (3.141592653589793 * delta4);

  const double *coords = fieldManager.getFieldAs<double>("Coords")->dataPtr();
  const double *volumes = fieldManager.getFieldAs<double>("Volume")->dataPtr();
  const double *tempPtr = fieldManager.getFieldAs<double>("Temperature")->dataPtr();
  const double *gradPtr = fieldManager.getFieldAs<double>("TempGradient")->dataPtr();
  const double *vvPtr = fieldManager.getFieldAs<double>("VHorizon")->dataPtr();
  const double *omegaPtr = neighborList.getBondFieldPtr("InfluenceWeight");
  double *ratePtr = fieldManager.getFieldAs<double>("TempRate")->dataPtr();

#pragma omp parallel for schedule(static)
  for (int i = 0; i < static_cast<int>(numParticles); ++i) {
    double xi_x = coords[i * 3];
    double xi_y = coords[i * 3 + 1];
    double xi_z = coords[i * 3 + 2];
    double ti = tempPtr[i];

    double grad_x = gradPtr[i * 3];
    double grad_y = gradPtr[i * 3 + 1];
    double grad_z = gradPtr[i * 3 + 2];

    double k_i = kArr[i];
    double G_i = g0_ * k_i;
    double coeff_i = coeff_base * G_i;
    double vv_i = vvPtr[i];

    double rate_sum_ze = 0.0;
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

      double deltaT_actual = tempPtr[j] - ti;
      double deltaT_predicted_plus = grad_x * dx + grad_y * dy + grad_z * dz;
      double deltaT_res_plus = deltaT_actual - deltaT_predicted_plus;

      double grad_xj = gradPtr[j * 3];
      double grad_yj = gradPtr[j * 3 + 1];
      double grad_zj = gradPtr[j * 3 + 2];
      double deltaT_predicted_minus = grad_xj * dx + grad_yj * dy + grad_zj * dz;
      double deltaT_res_minus = deltaT_actual - deltaT_predicted_minus;

      double k_j = kArr[j];
      double G_j = g0_ * k_j;
      double coeff_j = coeff_base * G_j;
      double vv_j = vvPtr[j];

      // Silling Penalty
      rate_sum_ze += omega * coeff_i * (deltaT_res_plus / vv_i) * vj;
      rate_sum_ze += omega * coeff_j * (deltaT_res_minus / vv_j) * vj;
    }
    
    // 注意：如果是用在 PDEngine 中，不需要手动除以 rho*cp，直接把热功率加进去即可
    // 但在原逻辑中 `rate_sum_ze` 乘以了 `thermal_coeff = 1.0 / (rho * cp)`
    // 因此我们需要查验调用本方法的外部是如何更新温度率的。这里我们加上直接累积：
    // TODO: 调用层应当保证外部加上外部热源之后统一除以 rho*cp
    ratePtr[i] += rate_sum_ze;
  }
}

// =========================================================================
// WanStabilizer 实现
// =========================================================================
void WanStabilizer::applyPenalty(PDContext &ctx) {
  auto &fieldManager = ctx.getFieldManager();
  auto &neighborList = ctx.getNeighborList();
  auto &manager = ctx.getParticleManager();

  const size_t numParticles = manager.getTotalParticles();
  std::vector<double> kArr = ExtractConductivityArray(manager);

  const double *coords = fieldManager.getFieldAs<double>("Coords")->dataPtr();
  const double *volumes = fieldManager.getFieldAs<double>("Volume")->dataPtr();
  const double *tempPtr = fieldManager.getFieldAs<double>("Temperature")->dataPtr();
  const double *gradPtr = fieldManager.getFieldAs<double>("TempGradient")->dataPtr();
  const double *shapeInvPtr = fieldManager.getFieldAs<double>("ShapeTensorInv")->dataPtr();
  const double *omegaPtr = neighborList.getBondFieldPtr("InfluenceWeight");
  double *ratePtr = fieldManager.getFieldAs<double>("TempRate")->dataPtr();

#pragma omp parallel for schedule(static)
  for (int i = 0; i < static_cast<int>(numParticles); ++i) {
    double xi_x = coords[i * 3];
    double xi_y = coords[i * 3 + 1];
    double xi_z = coords[i * 3 + 2];
    double ti = tempPtr[i];

    double grad_x = gradPtr[i * 3];
    double grad_y = gradPtr[i * 3 + 1];
    double grad_z = gradPtr[i * 3 + 2];

    double trace_K_inv_i = shapeInvPtr[i * 9] + shapeInvPtr[i * 9 + 4] + shapeInvPtr[i * 9 + 8];
    double G_Wan_i = g0_ * kArr[i] * trace_K_inv_i;

    double rate_sum_ze = 0.0;
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

      double deltaT_actual = tempPtr[j] - ti;
      double deltaT_predicted_plus = grad_x * dx + grad_y * dy + grad_z * dz;
      double deltaT_res_plus = deltaT_actual - deltaT_predicted_plus;

      double grad_xj = gradPtr[j * 3];
      double grad_yj = gradPtr[j * 3 + 1];
      double grad_zj = gradPtr[j * 3 + 2];
      double deltaT_predicted_minus = grad_xj * dx + grad_yj * dy + grad_zj * dz;
      double deltaT_res_minus = deltaT_actual - deltaT_predicted_minus;

      double trace_K_inv_j = shapeInvPtr[j * 9] + shapeInvPtr[j * 9 + 4] + shapeInvPtr[j * 9 + 8];
      double G_Wan_j = g0_ * kArr[j] * trace_K_inv_j;

      // Wan Penalty
      rate_sum_ze += omega * G_Wan_i * deltaT_res_plus * vj;
      rate_sum_ze += omega * G_Wan_j * deltaT_res_minus * vj;
    }
    ratePtr[i] += rate_sum_ze;
  }
}

// =========================================================================
// ZhangStabilizer 实现
// =========================================================================
void ZhangStabilizer::preCompute(PDContext &ctx) {
  auto &fieldManager = ctx.getFieldManager();
  auto &manager = ctx.getParticleManager();
  const size_t numParticles = manager.getTotalParticles();

  std::vector<double> kArr = ExtractConductivityArray(manager);

  auto &reg = PDCommon::Field::FieldRegistry::getInstance();
  auto ktFieldNew = reg.createField("DoubleField", "KT_Tensor", 9);
  fieldManager.addField(std::move(ktFieldNew));
  auto *ktField = fieldManager.getFieldAs<double>("KT_Tensor");
  ktField->resize(numParticles);
  ktField->clearToZero();

  double *ktPtr = ktField->dataPtr();
  const double *shapeInvPtr = fieldManager.getFieldAs<double>("ShapeTensorInv")->dataPtr();
  
#pragma omp parallel for schedule(static)
  for (int i = 0; i < static_cast<int>(numParticles); ++i) {
    double k = kArr[i];
    
    int idx9 = i * 9;
    double i_k00 = shapeInvPtr[idx9],     i_k01 = shapeInvPtr[idx9 + 1], i_k02 = shapeInvPtr[idx9 + 2];
    double i_k10 = shapeInvPtr[idx9 + 3], i_k11 = shapeInvPtr[idx9 + 4], i_k12 = shapeInvPtr[idx9 + 5];
    double i_k20 = shapeInvPtr[idx9 + 6], i_k21 = shapeInvPtr[idx9 + 7], i_k22 = shapeInvPtr[idx9 + 8];
    
    ktPtr[idx9]     = k * (i_k00 * i_k00 + i_k01 * i_k10 + i_k02 * i_k20);
    ktPtr[idx9 + 1] = k * (i_k00 * i_k01 + i_k01 * i_k11 + i_k02 * i_k21);
    ktPtr[idx9 + 2] = k * (i_k00 * i_k02 + i_k01 * i_k12 + i_k02 * i_k22);
    
    ktPtr[idx9 + 3] = k * (i_k10 * i_k00 + i_k11 * i_k10 + i_k12 * i_k20);
    ktPtr[idx9 + 4] = k * (i_k10 * i_k01 + i_k11 * i_k11 + i_k12 * i_k21);
    ktPtr[idx9 + 5] = k * (i_k10 * i_k02 + i_k11 * i_k12 + i_k12 * i_k22);
    
    ktPtr[idx9 + 6] = k * (i_k20 * i_k00 + i_k21 * i_k10 + i_k22 * i_k20);
    ktPtr[idx9 + 7] = k * (i_k20 * i_k01 + i_k21 * i_k11 + i_k22 * i_k21);
    ktPtr[idx9 + 8] = k * (i_k20 * i_k02 + i_k21 * i_k12 + i_k22 * i_k22);
  }
  LOG_INFO("[ZhangStabilizer] Precomputed KT_Tensor globally.");
}

void ZhangStabilizer::applyPenalty(PDContext &ctx) {
  auto &fieldManager = ctx.getFieldManager();
  auto &neighborList = ctx.getNeighborList();
  auto &manager = ctx.getParticleManager();

  const size_t numParticles = manager.getTotalParticles();

  const double *coords = fieldManager.getFieldAs<double>("Coords")->dataPtr();
  const double *volumes = fieldManager.getFieldAs<double>("Volume")->dataPtr();
  const double *tempPtr = fieldManager.getFieldAs<double>("Temperature")->dataPtr();
  const double *gradPtr = fieldManager.getFieldAs<double>("TempGradient")->dataPtr();
  const double *vvPtr = fieldManager.getFieldAs<double>("VHorizon")->dataPtr();
  const double *ktPtr = fieldManager.getFieldAs<double>("KT_Tensor")->dataPtr();
  const double *omegaPtr = neighborList.getBondFieldPtr("InfluenceWeight");
  double *ratePtr = fieldManager.getFieldAs<double>("TempRate")->dataPtr();

#pragma omp parallel for schedule(static)
  for (int i = 0; i < static_cast<int>(numParticles); ++i) {
    double xi_x = coords[i * 3];
    double xi_y = coords[i * 3 + 1];
    double xi_z = coords[i * 3 + 2];
    double ti = tempPtr[i];

    double grad_x = gradPtr[i * 3];
    double grad_y = gradPtr[i * 3 + 1];
    double grad_z = gradPtr[i * 3 + 2];

    double vv_i = vvPtr[i];
    int idx9_i = i * 9;
    double KT_00 = ktPtr[idx9_i],     KT_01 = ktPtr[idx9_i + 1], KT_02 = ktPtr[idx9_i + 2];
    double KT_10 = ktPtr[idx9_i + 3], KT_11 = ktPtr[idx9_i + 4], KT_12 = ktPtr[idx9_i + 5];
    double KT_20 = ktPtr[idx9_i + 6], KT_21 = ktPtr[idx9_i + 7], KT_22 = ktPtr[idx9_i + 8];

    double rate_sum_ze = 0.0;
    const int numNeighbors = neighborList.getNeighborCount(i);
    const int *neighbors = neighborList.getNeighborIds(i);
    const int offset = neighbors - neighborList.getNeighborIds(0);

    for (int k_nb = 0; k_nb < numNeighbors; ++k_nb) {
      int j = neighbors[k_nb];
      if (j == -1) continue;

      double dx = coords[j * 3] - xi_x;
      double dy = coords[j * 3 + 1] - xi_y;
      double dz = coords[j * 3 + 2] - xi_z;
      double vj = volumes[j];
      double omega = omegaPtr[offset + k_nb];

      double deltaT_actual = tempPtr[j] - ti;
      double deltaT_predicted_plus = grad_x * dx + grad_y * dy + grad_z * dz;
      double deltaT_res_plus = deltaT_actual - deltaT_predicted_plus;

      double grad_xj = gradPtr[j * 3];
      double grad_yj = gradPtr[j * 3 + 1];
      double grad_zj = gradPtr[j * 3 + 2];
      double deltaT_predicted_minus = grad_xj * dx + grad_yj * dy + grad_zj * dz;
      double deltaT_res_minus = deltaT_actual - deltaT_predicted_minus;

      // T_i 惩罚项
      double xi_KT_x = KT_00 * dx + KT_01 * dy + KT_02 * dz;
      double xi_KT_y = KT_10 * dx + KT_11 * dy + KT_12 * dz;
      double xi_KT_z = KT_20 * dx + KT_21 * dy + KT_22 * dz;
      double Z_i = omega * omega * (dx * xi_KT_x + dy * xi_KT_y + dz * xi_KT_z);
      double G_Zhang_i = Z_i * vj * vvPtr[j];
      
      // T_j 惩罚项
      int idx9_j = j * 9;
      double KT_j_00 = ktPtr[idx9_j],     KT_j_01 = ktPtr[idx9_j + 1], KT_j_02 = ktPtr[idx9_j + 2];
      double KT_j_10 = ktPtr[idx9_j + 3], KT_j_11 = ktPtr[idx9_j + 4], KT_j_12 = ktPtr[idx9_j + 5];
      double KT_j_20 = ktPtr[idx9_j + 6], KT_j_21 = ktPtr[idx9_j + 7], KT_j_22 = ktPtr[idx9_j + 8];

      double j_xi_KT_x = KT_j_00 * dx + KT_j_01 * dy + KT_j_02 * dz;
      double j_xi_KT_y = KT_j_10 * dx + KT_j_11 * dy + KT_j_12 * dz;
      double j_xi_KT_z = KT_j_20 * dx + KT_j_21 * dy + KT_j_22 * dz;
      double Z_j = omega * omega * (dx * j_xi_KT_x + dy * j_xi_KT_y + dz * j_xi_KT_z);
      
      double G_Zhang_j = Z_j * vj * vv_i;
      
      rate_sum_ze += g0_ * G_Zhang_i * deltaT_res_plus;
      rate_sum_ze += g0_ * G_Zhang_j * deltaT_res_minus;
    }
    ratePtr[i] += rate_sum_ze;
  }
}

} // namespace PDCommon::Kernel
