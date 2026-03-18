// ============================================================================
// NOSB_T.cpp - 热传导非常规态基近场动力学 (Thermal NOSB-PD)
// 
// 核心贡献：
//   无状态求解框架，专注于高性能的纯数学/空间积分。所有状态均托管在 PDContext。
//
// HPC 架构优势：
//   1. 极速邻域遍历：基于 1D CSR 扁平格式 (offsets+neighborIds)。
//   2. 极速断键跳过：利用 if (j == -1) continue; 无分支重惩罚跳过断键。
//   3. 内存连续访问：1D 扁平数组 (SoA) + #pragma omp parallel for schedule(static)。
//   4. 全局数据零拷贝：K⁻¹ 通过 9 分量场全局共享，利用 Eigen::Map 零拷贝读写。
// ============================================================================

#include "NOSB_T.h"
#include "FieldManager.h"
#include "Logger.h"
#include "NeighborList.h"
#include "ParticleManager.h"
#include "ThermalMaterial.h"
#include <omp.h>
#include <cmath>

namespace Src::Solve {

using namespace PDCommon::Core;
using namespace PDCommon::Model;
using namespace PDCommon::Field;
using namespace PDCommon::Material;
using namespace Eigen;

// ---------------------------------------------------------------------------
// 预计算：形状张量的逆 K⁻¹，仅在时间循环外调用一次
// ---------------------------------------------------------------------------
void NOSB_T::ComputeShapeTensors(PDContext& ctx) {
    auto& manager = ctx.getParticleManager();
    auto& neighborList = ctx.getNeighborList();
    auto& fieldManager = ctx.getFieldManager();

    const size_t numParticles = manager.getTotalParticles();

    // ===================================================================
    // 集中注册 NOSB_T 算法框架所需的全部工作场（仅此一次）
    // ===================================================================
    auto* shapeInvField = fieldManager.registerField<double>("ShapeTensorInv", 9);
    auto* tempGradField = fieldManager.registerField<double>("TempGradient", 3);
    auto* heatFluxField = fieldManager.registerField<double>("HeatFluxVec", 3);

    shapeInvField->resize(numParticles);
    tempGradField->resize(numParticles);
    heatFluxField->resize(numParticles);

    shapeInvField->clearToZero();

    // 提取高性能 SoA 裸指针
    double* shapeInvPtr = shapeInvField->dataPtr();
    auto* coordsField = fieldManager.getFieldAs<double>("Coords");
    auto* volumeField = fieldManager.getFieldAs<double>("Volume");
    if (!coordsField || !volumeField) {
        LOG_ERROR("[NOSB_T] Critical: Coords or Volume field missing in FieldManager!");
        return;
    }
    const double* coords = coordsField->dataPtr();
    const double* volumes = volumeField->dataPtr();

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
        const int* neighbors = neighborList.getNeighborIds(i);
        const double* bondLens = neighborList.getBondLengths(i);

        // ===================================================================
        // [HPC 优势] 纯局部邻居遍历，1D 数组顺序访问
        // ===================================================================
        for (int k = 0; k < numNeighbors; ++k) {
            int j = neighbors[k];

            // [HPC 优势] 遇到断键 (-1) 极速跳过
            if (j == -1) continue;

            Vector3d xj(coords[j * 3], coords[j * 3 + 1], coords[j * 3 + 2]);
            Vector3d deltaX = xj - xi;

            // 影响函数 omega(xi) = 1.0 / |xi|，其中 |xi| 是初始键长
            double omega = 1.0 / bondLens[k];
            double vj = volumes[j];

            // 积分累加：K_i += omega * (deltaX ⊗ deltaX) * Vj
            K += omega * (deltaX * deltaX.transpose()) * vj;
        }

        // 求逆计算
        // 备注：如果矩阵不可逆（例如表面孤立粒子或积分域过小），这里可加伪逆判断或惩罚，当前使用纯逆
        Matrix3d K_inv = K.inverse();
        
        // [HPC 优势] 使用 Eigen::Map 零拷贝直接覆写到底层 double[9] 连续内存中
        Map<Matrix<double, 3, 3, RowMajor>> K_inv_map(&shapeInvPtr[i * 9]);
        K_inv_map = K_inv;
    }
    
    LOG_INFO("[NOSB_T] All NOSB fields registered and Shape Tensor Inverse computed.");
}

// ---------------------------------------------------------------------------
// 核心三步：Thermal NOSB 态基积分算法（每个时间步调用）
// ---------------------------------------------------------------------------
void NOSB_T::ComputeThermalState(PDContext& ctx) {
    auto& manager = ctx.getParticleManager();
    auto& neighborList = ctx.getNeighborList();
    auto& fieldManager = ctx.getFieldManager();
    auto& matManager = ctx.getMaterialManager();

    const size_t numParticles = manager.getTotalParticles();

    // 1. 获取核心物理场
    auto* tempField = fieldManager.getFieldAs<double>("Temperature");
    auto* rateField = fieldManager.getFieldAs<double>("TempRate");
    auto* shapeInvField = fieldManager.getFieldAs<double>("ShapeTensorInv");

    if (!shapeInvField) {
        LOG_ERROR("[NOSB_T] 'ShapeTensorInv' not found! Please run ComputeShapeTensors() before solving.");
        return;
    }

    // 2. 获取 NOSB 专属的工作场（已在 ComputeShapeTensors 中统一注册）
    auto* tempGradField = fieldManager.getFieldAs<double>("TempGradient");
    auto* heatFluxField = fieldManager.getFieldAs<double>("HeatFluxVec");

    if (!tempGradField || !heatFluxField) {
        LOG_ERROR("[NOSB_T] 'TempGradient' or 'HeatFluxVec' not found! Please run ComputeShapeTensors() first.");
        return;
    }
    
    // 清空当前步的增量
    rateField->clearToZero();

    // 3. 提取所有高性能 SoA 裸指针
    auto* coordsField = fieldManager.getFieldAs<double>("Coords");
    auto* volumeField = fieldManager.getFieldAs<double>("Volume");
    if (!coordsField || !volumeField) {
        LOG_ERROR("[NOSB_T] Critical: Coords or Volume field missing in FieldManager!");
        return;
    }
    const double* coords = coordsField->dataPtr();
    const double* volumes = volumeField->dataPtr();
    
    const double* tempPtr = tempField->dataPtr();
    const double* shapeInvPtr = shapeInvField->dataPtr();
    
    double* gradPtr = tempGradField->dataPtr();
    double* fluxPtr = heatFluxField->dataPtr();
    double* ratePtr = rateField->dataPtr();

    // 用于访问每颗粒子的绑定的具体 ThermalMaterial
    const auto& particles = manager.getAllParticles();

    // =======================================================================
    // 步骤 1: 非局部温度梯度重构
    // 公式: ∇T_i = K_i⁻¹ * [ sum_j ω(ξ) * (T_j - T_i) * ΔX * V_j ]
    // =======================================================================
    #pragma omp parallel for schedule(static)
    for (int i = 0; i < static_cast<int>(numParticles); ++i) {
        Vector3d xi(coords[i * 3], coords[i * 3 + 1], coords[i * 3 + 2]);
        double ti = tempPtr[i];
        Vector3d integral_sum = Vector3d::Zero();

        const int numNeighbors = neighborList.getNeighborCount(i);
        const int* neighbors = neighborList.getNeighborIds(i);
        const double* bondLens = neighborList.getBondLengths(i);

        for (int k = 0; k < numNeighbors; ++k) {
            int j = neighbors[k];
            if (j == -1) continue; // 极速跳过断键

            Vector3d xj(coords[j * 3], coords[j * 3 + 1], coords[j * 3 + 2]);
            double tj = tempPtr[j];

            Vector3d deltaX = xj - xi;
            double deltaT = tj - ti;
            double omega = 1.0 / bondLens[k];
            double vj = volumes[j];

            integral_sum += omega * deltaT * deltaX * vj;
        }

        // 提取已预计算好的 K_i⁻¹ (利用 Eigen::Map 零拷贝)
        Map<const Matrix<double, 3, 3, RowMajor>> K_inv_i(&shapeInvPtr[i * 9]);
        
        // 算出真实的温度梯度场
        Vector3d gradT = K_inv_i * integral_sum;

        gradPtr[i * 3]     = gradT.x();
        gradPtr[i * 3 + 1] = gradT.y();
        gradPtr[i * 3 + 2] = gradT.z();
    }

    // =======================================================================
    // 步骤 2: 纯局部计算，调用材质黑盒
    // 公式: q_i = material->ComputeHeatFlux(∇T_i)
    // 优势：绝对解耦，NOSB 不关心是傅里叶还是非线性导热，只管传入梯度即可。
    // =======================================================================
    #pragma omp parallel for schedule(static)
    for (int i = 0; i < static_cast<int>(numParticles); ++i) {
        PDCommon::Material::Material* rawMat = particles[i].getMaterial();
        auto* thermalMat = dynamic_cast<ThermalMaterial*>(rawMat);
        
        if (thermalMat) {
            Vector3d gradT(gradPtr[i * 3], gradPtr[i * 3 + 1], gradPtr[i * 3 + 2]);
            
            // ---> [纯数学本构黑盒调用] <---
            Vector3d q = thermalMat->ComputeHeatFlux(gradT);
            
            fluxPtr[i * 3]     = q.x();
            fluxPtr[i * 3 + 1] = q.y();
            fluxPtr[i * 3 + 2] = q.z();
        } else {
            fluxPtr[i * 3]     = 0.0;
            fluxPtr[i * 3 + 1] = 0.0;
            fluxPtr[i * 3 + 2] = 0.0;
        }
    }

    // =======================================================================
    // 步骤 3: 非局部热量积分，计算真实温度变化率 $\dot{T}$
    // 公式: $\dot{T}_i = \frac{1}{\rho_i c_{p,i}} \sum_j \omega(\xi) \cdot (K_i⁻¹ q_i + K_j⁻¹ q_j) \cdot \Delta X \cdot V_j$
    // =======================================================================
    #pragma omp parallel for schedule(static)
    for (int i = 0; i < static_cast<int>(numParticles); ++i) {
        PDCommon::Material::Material* rawMat = particles[i].getMaterial();
        auto* thermalMat = dynamic_cast<ThermalMaterial*>(rawMat);
        if (!thermalMat) continue;

        double rho = thermalMat->getDensity();
        double cp  = thermalMat->getHeatCapacity();
        double thermal_coeff = 1.0 / (rho * cp);

        Vector3d xi(coords[i * 3], coords[i * 3 + 1], coords[i * 3 + 2]);
        Vector3d qi(fluxPtr[i * 3], fluxPtr[i * 3 + 1], fluxPtr[i * 3 + 2]);
        
        // $K_i⁻¹ q_i$
        Map<const Matrix<double, 3, 3, RowMajor>> K_inv_i(&shapeInvPtr[i * 9]);
        Vector3d KQ_i = K_inv_i * qi;

        double rate_sum = 0.0;

        const int numNeighbors = neighborList.getNeighborCount(i);
        const int* neighbors = neighborList.getNeighborIds(i);
        const double* bondLens = neighborList.getBondLengths(i);

        for (int k = 0; k < numNeighbors; ++k) {
            int j = neighbors[k];
            if (j == -1) continue;

            Vector3d xj(coords[j * 3], coords[j * 3 + 1], coords[j * 3 + 2]);
            Vector3d qj(fluxPtr[j * 3], fluxPtr[j * 3 + 1], fluxPtr[j * 3 + 2]);
            
            // $K_j⁻¹ q_j$
            Map<const Matrix<double, 3, 3, RowMajor>> K_inv_j(&shapeInvPtr[j * 9]);
            Vector3d KQ_j = K_inv_j * qj;

            Vector3d deltaX = xj - xi;
            double omega = 1.0 / bondLens[k];
            double vj = volumes[j];

            // 点乘累加热流转移
            rate_sum += omega * (KQ_i + KQ_j).dot(deltaX) * vj;
        }

        // 将最终积分结果赋予温度率 TempRate
        ratePtr[i] += thermal_coeff * rate_sum; 
    }
}

} // namespace Src::Solve
