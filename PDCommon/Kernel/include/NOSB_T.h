#ifndef PDCOMMON_KERNEL_NOSB_T_H
#define PDCOMMON_KERNEL_NOSB_T_H

// ============================================================================
// NOSB_T.h - 热传导非常规态基近场动力学 (Thermal NOSB-PD)
//
// 职责：
//   实现 Thermal NOSB-PD 的非局部积分计算框架。
//   将 CSR 格式邻居表上的非局部积分与本构黑盒 (ThermalMaterial) 桥接。
//
// 架构关系（三层多态 L2）：
//   PDKernel (L2 基类)
//     └── NOSB_T（本类）
//           ├── 读取 PDContext 中的物理场、邻居表、粒子数据
//           ├── 执行 NOSB 三步非局部积分
//           └── 调用 ThermalMaterial::ComputeHeatFlux() 完成本构计算 (L3)
//
// HPC 规约：
//   - 邻居表采用 1D CSR 扁平格式（offsets + neighborIds + bondLengths）
//   - 断键机制：neighborIds[k] == -1 表示已断键，遍历时跳过
//   - 循环粒子遍历使用 OpenMP 并行加速
//   - 使用 Eigen::Vector3d / Eigen::Matrix3d 进行向量/张量运算
//
// 本构解耦：
//   本类不包含任何材料参数或本构定律，纯粹是非局部积分框架。
//   本构计算通过 ThermalMaterial 基类的虚函数多态调用。
// ============================================================================

#include "NOSB_Base.h"
#include "Stabilizer.h"
#include "ThermalMaterial.h"
#include <Eigen/Dense>
#include <memory>

namespace PDCommon::Kernel {

/// @brief 热传导非常规态基近场动力学 (Thermal NOSB-PD) 计算框架
class NOSB_T : public NOSB_Base {
public:
  NOSB_T() = default;
  ~NOSB_T() override = default;

  // -----------------------------------------------------------------------
  // PDKernel 接口实现
  // -----------------------------------------------------------------------

  /// @brief 时间循环前预计算：计算形状张量 K⁻¹ 并注册工作场
  void preCompute(PDCommon::Core::PDContext &ctx) override;

  /// @brief 每步核心积分：NOSB 三步非局部热传导计算
  void computeForceState(PDCommon::Core::PDContext &ctx) override;

  /// @brief 返回此核心需要时间积分的场
  std::vector<IntegrationTarget> getIntegrationTargets() const override;

private:
  std::unique_ptr<Stabilizer> stabilizer_;

  // =======================================================================
  // HPC 优化缓存：避免每次调用堆分配，避免内层循环重复运算
  // =======================================================================
  std::vector<PDCommon::Material::ThermalMaterial *> matArrCache_;
  std::vector<double> rhoArrCache_;
  std::vector<double> cpArrCache_;
  std::vector<double> kArrCache_;
  // 预计算的矢量 KQ = K^-1 * q，容量 numParticles * 3
  std::vector<double> kqCache_;

  // -----------------------------------------------------------------------
  // 内部实现方法
  // -----------------------------------------------------------------------

  /// @brief 执行 Thermal NOSB-PD 完整三步积分
  void ComputeThermalState(PDCommon::Core::PDContext &ctx);
};

} // namespace PDCommon::Kernel

#endif // PDCOMMON_KERNEL_NOSB_T_H
