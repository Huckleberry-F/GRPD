#include "ThermalStabilizers.h"
#include "FieldManager.h"
#include "FieldRegistry.h"
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
    if (mat)
      kArr[i] = mat->getConductivity();
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
  const double *tempPtr =
      fieldManager.getFieldAs<double>("Temperature")->dataPtr();
  const double *gradPtr =
      fieldManager.getFieldAs<double>("TempGradient")->dataPtr();
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
      if (j == -1)
        continue;

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
      double deltaT_predicted_minus =
          grad_xj * dx + grad_yj * dy + grad_zj * dz;
      double deltaT_res_minus = deltaT_actual - deltaT_predicted_minus;

      double k_j = kArr[j];
      double G_j = g0_ * k_j;
      double coeff_j = coeff_base * G_j;
      double vv_j = vvPtr[j];

      // Silling Penalty
      rate_sum_ze += omega * coeff_i / horizon * (deltaT_res_plus / vv_i) * vj;
      rate_sum_ze += omega * coeff_j / horizon * (deltaT_res_minus / vv_j) * vj;
    }

    // 注意：如果是用在 PDEngine 中，不需要手动除以
    // rho*cp，直接把热功率加进去即可 但在原逻辑中 `rate_sum_ze` 乘以了
    // `thermal_coeff = 1.0 / (rho * cp)`
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
  const double *tempPtr =
      fieldManager.getFieldAs<double>("Temperature")->dataPtr();
  const double *gradPtr =
      fieldManager.getFieldAs<double>("TempGradient")->dataPtr();
  const double *shapeInvPtr =
      fieldManager.getFieldAs<double>("ShapeTensorInv")->dataPtr();
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

    double trace_K_inv_i =
        shapeInvPtr[i * 9] + shapeInvPtr[i * 9 + 4] + shapeInvPtr[i * 9 + 8];
    double G_Wan_i = g0_ * kArr[i] * trace_K_inv_i;

    double rate_sum_ze = 0.0;
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

      double omega = omegaPtr[offset + k_nb];
      double vj = volumes[j];

      double deltaT_actual = tempPtr[j] - ti;
      double deltaT_predicted_plus = grad_x * dx + grad_y * dy + grad_z * dz;
      double deltaT_res_plus = deltaT_actual - deltaT_predicted_plus;

      double grad_xj = gradPtr[j * 3];
      double grad_yj = gradPtr[j * 3 + 1];
      double grad_zj = gradPtr[j * 3 + 2];
      double deltaT_predicted_minus =
          grad_xj * dx + grad_yj * dy + grad_zj * dz;
      double deltaT_res_minus = deltaT_actual - deltaT_predicted_minus;

      double trace_K_inv_j =
          shapeInvPtr[j * 9] + shapeInvPtr[j * 9 + 4] + shapeInvPtr[j * 9 + 8];
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
  std::vector<double> kArr = ExtractConductivityArray(manager);
  const double *shapeInvPtr =
      fieldManager.getFieldAs<double>("ShapeTensorInv")->dataPtr();

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
    double i_k00 = shapeInvPtr[idx9_i], i_k01 = shapeInvPtr[idx9_i + 1],
           i_k02 = shapeInvPtr[idx9_i + 2];
    double i_k10 = shapeInvPtr[idx9_i + 3], i_k11 = shapeInvPtr[idx9_i + 4],
           i_k12 = shapeInvPtr[idx9_i + 5];
    double i_k20 = shapeInvPtr[idx9_i + 6], i_k21 = shapeInvPtr[idx9_i + 7],
           i_k22 = shapeInvPtr[idx9_i + 8];
    double k_i = kArr[i];

    double rate_sum_ze = 0.0;
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
      double vj = volumes[j];
      double omega = omegaPtr[offset + k_nb];

      double deltaT_actual = tempPtr[j] - ti;
      double deltaT_predicted_plus = grad_x * dx + grad_y * dy + grad_z * dz;
      double deltaT_res_plus = deltaT_actual - deltaT_predicted_plus;

      double grad_xj = gradPtr[j * 3];
      double grad_yj = gradPtr[j * 3 + 1];
      double grad_zj = gradPtr[j * 3 + 2];
      double deltaT_predicted_minus =
          grad_xj * dx + grad_yj * dy + grad_zj * dz;
      double deltaT_res_minus = deltaT_actual - deltaT_predicted_minus;

      double o2 = omega * omega;
      // ==========================================
      // 核心替换：点 i 的惩罚项 (利用 p_i = K_i^{-1} * xi)
      // ==========================================
      double px_i = i_k00 * dx + i_k01 * dy + i_k02 * dz;
      double py_i = i_k10 * dx + i_k11 * dy + i_k12 * dz;
      double pz_i = i_k20 * dx + i_k21 * dy + i_k22 * dz;
      double p_dot_p_i = px_i * px_i + py_i * py_i + pz_i * pz_i;
      double Z_i = o2 * k_i * p_dot_p_i;
      double G_Zhang_i = Z_i * vj * vvPtr[j];

      // ==========================================
      // 核心替换：点 j 的惩罚项 (利用 p_j = K_j^{-1} * xi)
      // ==========================================
      int idx9_j = j * 9;
      // 实时读取点 j 的逆形状张量
      double j_k00 = shapeInvPtr[idx9_j], j_k01 = shapeInvPtr[idx9_j + 1],
             j_k02 = shapeInvPtr[idx9_j + 2];
      double j_k10 = shapeInvPtr[idx9_j + 3], j_k11 = shapeInvPtr[idx9_j + 4],
             j_k12 = shapeInvPtr[idx9_j + 5];
      double j_k20 = shapeInvPtr[idx9_j + 6], j_k21 = shapeInvPtr[idx9_j + 7],
             j_k22 = shapeInvPtr[idx9_j + 8];
      double k_j = kArr[j];

      double px_j = j_k00 * dx + j_k01 * dy + j_k02 * dz;
      double py_j = j_k10 * dx + j_k11 * dy + j_k12 * dz;
      double pz_j = j_k20 * dx + j_k21 * dy + j_k22 * dz;
      double p_dot_p_j = px_j * px_j + py_j * py_j + pz_j * pz_j;
      double Z_j = o2 * k_j * p_dot_p_j;
      double G_Zhang_j = Z_j * vj * vv_i;

      rate_sum_ze += g0_ * G_Zhang_i * deltaT_res_plus;
      rate_sum_ze += g0_ * G_Zhang_j * deltaT_res_minus;
    }
    ratePtr[i] += rate_sum_ze;
  }
}

} // namespace PDCommon::Kernel
