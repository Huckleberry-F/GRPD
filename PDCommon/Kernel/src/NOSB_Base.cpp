#include "NOSB_Base.h"
#include "FieldManager.h"
#include "FieldRegistry.h"
#include "Logger.h"
#include "NeighborList.h"
#include "ParticleManager.h"
#include <cmath>
#include <omp.h>

namespace PDCommon::Kernel {

using namespace PDCommon::Core;
using namespace PDCommon::Field;
using namespace Eigen;

// ---------------------------------------------------------------------------
// NOSB 家族通用 YAML 配置解析
// ---------------------------------------------------------------------------
void NOSB_Base::configure(const YAML::Node &solverNode) {
  // 1. 核函数类型
  if (solverNode["KernelWeightType"]) {
    std::string kt = solverNode["KernelWeightType"].as<std::string>();
    if (kt == "Constant")
      kernelType_ = InfluenceKernelType::Constant;
    else if (kt == "Linear")
      kernelType_ = InfluenceKernelType::Linear;
    else if (kt == "Quadratic")
      kernelType_ = InfluenceKernelType::Quadratic;
    else if (kt == "Cubic")
      kernelType_ = InfluenceKernelType::Cubic;
    else if (kt == "Quartic")
      kernelType_ = InfluenceKernelType::Quartic;
    else if (kt == "Gaussian")
      kernelType_ = InfluenceKernelType::Gaussian;
    else
      kernelType_ = InfluenceKernelType::InverseDistance;
    LOG_INFO("[NOSB_Base] Applied KernelWeightType: " + kt);
  }
  // 2. 零能修正系数
  if (solverNode["ZeroEnergyG0"]) {
    zeroEnergyG0_ = solverNode["ZeroEnergyG0"].as<double>();
    LOG_INFO("[NOSB_Base] Applied ZeroEnergyG0: " +
             std::to_string(zeroEnergyG0_));
  }
  // 3. 零能修正方案配置字符串
  if (solverNode["ZeroEnergyMethod"]) {
    zeroEnergyMethodStr_ = solverNode["ZeroEnergyMethod"].as<std::string>();
    LOG_INFO("[NOSB_Base] Applied ZeroEnergyMethod string: " +
             zeroEnergyMethodStr_);
  }
  // 4. 是否动态刷新形状张量
  if (solverNode["DynamicShapeTensor"]) {
    dynamicShapeTensor_ = solverNode["DynamicShapeTensor"].as<bool>();
    LOG_INFO("[NOSB_Base] Dynamic Shape Tensor update is set to: " +
             std::to_string(dynamicShapeTensor_));
  }
}

// ---------------------------------------------------------------------------
// 形状张量逆 K⁻¹ 计算
// ---------------------------------------------------------------------------
void NOSB_Base::ComputeShapeTensors(PDContext &ctx) {
  auto &manager = ctx.getParticleManager();
  auto &neighborList = ctx.getNeighborList();
  auto &fieldManager = ctx.getFieldManager();

  const size_t numParticles = manager.getTotalParticles();

  // ===================================================================
  // 集中注册 NOSB 算法框架所需的工作场（仅此一次）
  // ===================================================================
  auto &reg = FieldRegistry::getInstance();
  auto shapeField = reg.createField("DoubleField", "ShapeTensorInv", 9);
  auto vhField = reg.createField("DoubleField", "VHorizon", 1);
  auto sfField = reg.createField("DoubleField", "SumFac", 1);
  auto soField = reg.createField("DoubleField", "SumOmegaRaw", 1);
  fieldManager.addField(std::move(shapeField));
  fieldManager.addField(std::move(vhField));
  fieldManager.addField(std::move(sfField));
  fieldManager.addField(std::move(soField));
  auto *shapeInvField = fieldManager.getFieldAs<double>("ShapeTensorInv");
  auto *vHorizonField = fieldManager.getFieldAs<double>("VHorizon");
  auto *sumFacField = fieldManager.getFieldAs<double>("SumFac");
  auto *sumOmegaRawField = fieldManager.getFieldAs<double>("SumOmegaRaw");

  shapeInvField->resize(numParticles);
  vHorizonField->resize(numParticles);
  sumFacField->resize(numParticles);
  sumOmegaRawField->resize(numParticles);

  shapeInvField->clearToZero();
  vHorizonField->clearToZero();
  sumFacField->clearToZero();
  sumOmegaRawField->clearToZero();

  // 提取高性能 SoA 裸指针
  double *shapeInvPtr = shapeInvField->dataPtr();
  double *vvPtr = vHorizonField->dataPtr();
  double *sumFacPtr = sumFacField->dataPtr();
  double *sumOmegaRawPtr = sumOmegaRawField->dataPtr();
  auto *coordsField = fieldManager.getFieldAs<double>("Coords");
  auto *volumeField = fieldManager.getFieldAs<double>("Volume");
  if (!coordsField || !volumeField) {
    LOG_ERROR("[NOSB_Base] Critical: Coords or Volume field missing in "
              "FieldManager!");
    return;
  }
  const double *coords = coordsField->dataPtr();
  const double *volumes = volumeField->dataPtr();

  // ===================================================================
  // [HPC 优化] 预计算影响函数权重 BondField + 形状张量 K⁻¹（合并单次遍历）
  // omega 计算完立即存入 BondField，同时用于 K 累加，零额外遍历开销
  // ===================================================================
  neighborList.registerBondField("InfluenceWeight");
  double *omegaPtr = neighborList.getBondFieldPtr("InfluenceWeight");
  const double horizon = neighborList.getHorizon();

  // 从上下文获取模型维度和厚度
  const int dim = ctx.getDimension();
  const double thickness = ctx.getThickness();

  LOG_INFO(
      "[NOSB_Base] Computing InfluenceWeight + Shape Tensor Inverse (K^-1)...");

  // ===================================================================
  // [PASS 1] 计算影响函数权重 + 形状张量 K 累加 + K⁻¹
  // ===================================================================
  // 孤立粒子保护阈值：3D 至少需要 dim+1=4 个非共面邻居，2D 需要 3 个
  const int minNeighborsRequired = dim + 1;

#pragma omp parallel for schedule(static)
  for (int i = 0; i < static_cast<int>(numParticles); ++i) {
    const int numNeighbors = neighborList.getNeighborCount(i);
    const int *neighbors = neighborList.getNeighborIds(i);
    const double *bondLens = neighborList.getBondLengths(i);
    const int offset = neighbors - neighborList.getNeighborIds(0);

    // ---------------------------------------------------------------
    // 兜底保护：邻居数不足 → K 物理上不可能满秩，直接赋单位阵跳过
    // ---------------------------------------------------------------
    // 先统计有效邻居数（排除 j == -1 的无效邻居）
    int validNeighborCount = 0;
    for (int k = 0; k < numNeighbors; ++k) {
      if (neighbors[k] != -1)
        ++validNeighborCount;
    }

    if (validNeighborCount < minNeighborsRequired) {
      // omega 仍需写入 BondField（避免后续读取未初始化值）
      for (int k = 0; k < numNeighbors; ++k) {
        double omega_raw =
            GetInfluenceWeight(bondLens[k], horizon, kernelType_);
        // 2D: Volume = dx² * thickness → dx = sqrt(Volume/thickness)
        // 3D: Volume = dx³ → dx = cbrt(Volume)
        double dx_i = (dim == 2) ? std::sqrt(volumes[i] / thickness) : std::cbrt(volumes[i]);
        double radij = dx_i * 0.5;
        double fac = GetPartialVolumeFactor(bondLens[k], horizon, radij);
        double omega = omega_raw * fac;
        omegaPtr[offset + k] = omega;
      }
      vvPtr[i] = 1.0; // 防止除零
      Map<Matrix<double, 3, 3, RowMajor>> K_inv_map(&shapeInvPtr[i * 9]);
      K_inv_map = Matrix3d::Identity();
      continue;
    }

    // ---------------------------------------------------------------
    // 正常粒子：标准的 K 累加 (NOSB中无需基于体积的表面刚度修正)
    // ---------------------------------------------------------------
    Vector3d xi(coords[i * 3], coords[i * 3 + 1], coords[i * 3 + 2]);
    Matrix3d K = Matrix3d::Zero();
    double sum_fac = 0.0;       // Σ fac_ij（部分体积因子之和）
    double sum_omega_raw = 0.0; // Σ omega_raw_ij（原始核函数之和）

    for (int k = 0; k < numNeighbors; ++k) {
      int j = neighbors[k];

      // 计算 omega * fac 并存入 BondField
      double omega_raw = GetInfluenceWeight(bondLens[k], horizon, kernelType_);
      // 2D: Volume = dx² * thickness → dx = sqrt(Volume/thickness)
      // 3D: Volume = dx³ → dx = cbrt(Volume)
      double dx_i = (dim == 2) ? std::sqrt(volumes[i] / thickness) : std::cbrt(volumes[i]);
      double radij = dx_i * 0.5; // 粒子半径 = dx/2
      double fac = GetPartialVolumeFactor(bondLens[k], horizon, radij);
      double omega = omega_raw * fac;
      omegaPtr[offset + k] = omega;

      if (j == -1)
        continue;

      // 累加 VHorizon 分子分母
      sum_fac += fac;
      sum_omega_raw += omega_raw;

      Vector3d xj(coords[j * 3], coords[j * 3 + 1], coords[j * 3 + 2]);
      Vector3d deltaX = xj - xi;

      double vj = volumes[j];
      K += omega * (deltaX * deltaX.transpose()) * vj;
    }

    // 分别存储 Σfac 和 Σω_raw，供零能修正器灵活使用
    sumFacPtr[i] = sum_fac;
    sumOmegaRawPtr[i] = sum_omega_raw;
    // v_horizon = Σfac / Σω_raw（邻域部分体积修正比，保持兼容）
    vvPtr[i] = (sum_omega_raw > 1e-30) ? (sum_fac / sum_omega_raw) : 1.0;

    // 计算真实的几何有效迹以作为正则化基准（排除被硬编码垫乱的维度）
    double trace_K = (dim == 2) ? (K(0, 0) + K(1, 1)) : K.trace();

    // 通用正则化：基于迹(Trace)的动态小参量
    // 关键修复：将系数从 1e-6 提升至 2.5e-3。
    // 这能在保证连续体内部精度（误差<0.25%）的前提下，强制兜住预裂纹尖端和自由表面的奇异性。
    // 彻底解决 Zhang 氏零能修正中 (K_inv * dx)^2 导致的平方级引爆现象！
    K += (trace_K * 2.5e-3 + 1e-15) * Matrix3d::Identity();

    // 2D 降维保护：在正则化完成后，强制 Z 轴满秩，使 K_inv(2,2) = 1.0
    // 从而严格消除 Z 方向数值噪音（deltaZ 严格为 0，乘 1.0 结果仍为 0）
    if (dim == 2) {
      K(2, 0) = K(0, 2) = 0.0;
      K(2, 1) = K(1, 2) = 0.0;
      K(2, 2) = 1.0;
    }

    Matrix3d K_inv = K.inverse();
    Map<Matrix<double, 3, 3, RowMajor>> K_inv_map(&shapeInvPtr[i * 9]);
    K_inv_map = K_inv;
  }

  LOG_INFO("[NOSB_Base] All NOSB base fields registered and Shape Tensor "
           "Inverse computed.");
}

// ---------------------------------------------------------------------------
// 动态刷新形状张量逆 K⁻¹
// ---------------------------------------------------------------------------
void NOSB_Base::UpdateShapeTensors(PDContext &ctx) {
  auto &manager = ctx.getParticleManager();
  auto &neighborList = ctx.getNeighborList();
  auto &fieldManager = ctx.getFieldManager();

  const size_t numParticles = manager.getTotalParticles();

  auto *shapeInvField = fieldManager.getFieldAs<double>("ShapeTensorInv");
  auto *coordsField = fieldManager.getFieldAs<double>("Coords");
  auto *volumeField = fieldManager.getFieldAs<double>("Volume");
  auto *bondIntegrityField = fieldManager.getFieldAs<double>("BondIntegrity");
  auto *activeStatusField = fieldManager.getFieldAs<int>("ActiveStatus");

  if (!shapeInvField || !coordsField || !volumeField)
    return;

  double *shapeInvPtr = shapeInvField->dataPtr();
  const double *coords = coordsField->dataPtr();
  const double *volumes = volumeField->dataPtr();
  double *bondIntegrityPtr =
      bondIntegrityField ? bondIntegrityField->dataPtr() : nullptr;
  int *activeStatusPtr =
      activeStatusField ? activeStatusField->dataPtr() : nullptr;

  const double *omegaPtr = neighborList.getBondFieldPtr("InfluenceWeight");
  if (!omegaPtr)
    return;

  const int dim = ctx.getDimension();
  const int minNeighborsRequired = dim + 1;

#pragma omp parallel for schedule(static)
  for (int i = 0; i < static_cast<int>(numParticles); ++i) {
    // 若该粒子已被宣告死亡，则跳过计算以节省时间并防止奇异性
    if ((bondIntegrityPtr && bondIntegrityPtr[i] >= 0.99) ||
        (activeStatusPtr && activeStatusPtr[i] == 0)) {
      Map<Matrix<double, 3, 3, RowMajor>> K_inv_map(&shapeInvPtr[i * 9]);
      K_inv_map = Matrix3d::Identity();
      continue;
    }

    const int numNeighbors = neighborList.getNeighborCount(i);
    const int *neighbors = neighborList.getNeighborIds(i);
    const int offset = neighbors - neighborList.getNeighborIds(0);

    // 统计有效邻居数
    int validNeighborCount = 0;
    for (int k = 0; k < numNeighbors; ++k) {
      if (neighbors[k] != -1)
        ++validNeighborCount;
    }

    // 若判定存活键过少，无法维持连续体状态（K 退化），则底层击毙该点
    if (validNeighborCount < minNeighborsRequired) {
      Map<Matrix<double, 3, 3, RowMajor>> K_inv_map(&shapeInvPtr[i * 9]);
      K_inv_map = Matrix3d::Identity();
      if (bondIntegrityPtr) bondIntegrityPtr[i] = 1.0;
      if (activeStatusPtr) activeStatusPtr[i] = 0;
      continue;
    }

    // 重构新 K 矩阵
    Vector3d xi(coords[i * 3], coords[i * 3 + 1], coords[i * 3 + 2]);
    Matrix3d K = Matrix3d::Zero();

    for (int k = 0; k < numNeighbors; ++k) {
      int j = neighbors[k];
      if (j == -1)
        continue;

      double omega = omegaPtr[offset + k];
      Vector3d xj(coords[j * 3], coords[j * 3 + 1], coords[j * 3 + 2]);
      Vector3d deltaX = xj - xi;
      double vj = volumes[j];

      K += omega * (deltaX * deltaX.transpose()) * vj;
    }

    double trace_K = (dim == 2) ? (K(0, 0) + K(1, 1)) : K.trace();
    K += (trace_K * 2.5e-3 + 1e-15) * Matrix3d::Identity();

    if (dim == 2) {
      K(2, 0) = K(0, 2) = 0.0;
      K(2, 1) = K(1, 2) = 0.0;
      K(2, 2) = 1.0;
    }

    // 检查不可逆性
    if (std::abs(K.determinant()) < 1e-12) {
      Map<Matrix<double, 3, 3, RowMajor>> K_inv_map(&shapeInvPtr[i * 9]);
      K_inv_map = Matrix3d::Identity();
      if (bondIntegrityPtr) bondIntegrityPtr[i] = 1.0;
      if (activeStatusPtr) activeStatusPtr[i] = 0;
      continue;
    }

    Matrix3d K_inv = K.inverse();
    Map<Matrix<double, 3, 3, RowMajor>> K_inv_map(&shapeInvPtr[i * 9]);
    K_inv_map = K_inv;
  }
}

} // namespace PDCommon::Kernel
