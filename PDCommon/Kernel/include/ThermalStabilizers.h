#ifndef PDCOMMON_KERNEL_THERMAL_STABILIZERS_H
#define PDCOMMON_KERNEL_THERMAL_STABILIZERS_H

#include "PDContext.h"
#include "Stabilizer.h"
#include "StabilizerRegistry.h"
#include <vector>

namespace PDCommon::Kernel {

// =========================================================================
// 零能模式稳定化策略 (Hourglass Stabilization Strategies)
// 继承自全局泛型的 Stabilizer 基类，在 applyPenalty 内部独立获取热物理参数。
// =========================================================================

// =========================================================================
// Silling 方法：标量各向同性惩罚
// G = G0 * k, penalty = ω * (6k / πδ⁴) * (ΔTres / vv) * Vj
// =========================================================================
class SillingStabilizer : public Stabilizer {
public:
  void applyPenalty(PDCommon::Core::PDContext &ctx) override;
};

// =========================================================================
// Wan 方法：迹加权各向同性惩罚
// G = G0 * k * trace(K⁻¹), penalty = ω * G * ΔTres * Vj
// =========================================================================
class WanStabilizer : public Stabilizer {
public:
  void applyPenalty(PDCommon::Core::PDContext &ctx) override;
};

// =========================================================================
// Zhang 方法：各向异性惩罚 (KT = K⁻¹ · D · K⁻¹ 二次型)
// Z = ω² · ξᵀ · KT · ξ,  G_Zhang = Z * Vj * vv_j
// =========================================================================
class ZhangStabilizer : public Stabilizer {
public:
  void preCompute(PDCommon::Core::PDContext &ctx) override;
  void applyPenalty(PDCommon::Core::PDContext &ctx) override;
};

} // namespace PDCommon::Kernel

#endif // PDCOMMON_KERNEL_THERMAL_STABILIZERS_H
