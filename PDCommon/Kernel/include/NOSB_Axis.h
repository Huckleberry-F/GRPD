#ifndef PDCOMMON_KERNEL_NOSB_AXIS_H
#define PDCOMMON_KERNEL_NOSB_AXIS_H

// ============================================================================
// NOSB_Axis.h - 轴对称非常规态基近场动力学 (Axisymmetric NOSB-PD)
//
// 职责：
//   基于 NOSB_M 的 3x3 框架实现轴对称 NOSB-PD。
//   在 preCompute 阶段使用环向体积 (2πR·V) 重算 K^-1 覆写基类结果，
//   在力态积分中同样使用环向体积，确保量纲完全自洽。
//   仅覆写 F(2,2) = 1 + u_r/R 并追加 -P_θθ/R 修正项。
// ============================================================================

#include "NOSB_Base.h"
#include "Stabilizer.h"
#include "MechanicalMaterial.h"
#include <memory>
#include <vector>

namespace PDCommon::Kernel {

class NOSB_Axis : public NOSB_Base {
public:
  NOSB_Axis() = default;
  ~NOSB_Axis() override = default;

  void preCompute(PDCommon::Core::PDContext &ctx) override;
  void computeForceState(PDCommon::Core::PDContext &ctx) override;
  void postCompute(PDCommon::Core::PDContext &ctx) override;
  std::vector<IntegrationTarget> getIntegrationTargets() const override;

private:
  std::unique_ptr<Stabilizer> stabilizer_;

  // HPC 缓存
  std::vector<PDCommon::Material::MechanicalMaterial *> matArrCache_;
  std::vector<double> rhoArrCache_;
  std::vector<double> pkKinvCache_; // P * K^-1 缓存 (9 分量)
  std::vector<double> ringKinvCache_; // 专用于轴对称力循环的环向体积 K^-1


  /// @brief 完整的轴对称 3 步计算
  void ComputeAxisymmetricState(PDCommon::Core::PDContext &ctx);

  /// @brief 用环向体积 (2πR·V) 重新计算 K 和 K^-1，覆写基类 ShapeTensorInv
  void RecomputeShapeTensorsWithRingVolume(PDCommon::Core::PDContext &ctx,
                                           bool allowParticleDeactivation);
};

} // namespace PDCommon::Kernel

#endif // PDCOMMON_KERNEL_NOSB_AXIS_H
