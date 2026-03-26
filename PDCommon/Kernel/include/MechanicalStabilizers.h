#ifndef PDCOMMON_KERNEL_MECHANICAL_STABILIZERS_H
#define PDCOMMON_KERNEL_MECHANICAL_STABILIZERS_H

#include "PDContext.h"
#include "Stabilizer.h"
#include "StabilizerRegistry.h"
#include <vector>

namespace PDCommon::Kernel {

// =========================================================================
// 力学零能模式稳定化策略 (Mechanical Hourglass Stabilization Strategies)
// =========================================================================

// =========================================================================
// Silling 方法：标量各向同性惩罚 (Silling, 2015)
// f_ze = ω * (18 K / π δ⁴) * (z_i / vv_i - z_j / vv_j) * Vj
// =========================================================================
class MechanicalSillingStabilizer : public Stabilizer {
public:
  void applyPenalty(PDCommon::Core::PDContext &ctx) override;
};

// =========================================================================
// Wan 方法：迹加权各向同性惩罚
// G = G0 * K * trace(K⁻¹), penalty = ω * (G_i z_i - G_j z_j) * Vj
// =========================================================================
class MechanicalWanStabilizer : public Stabilizer {
public:
  void applyPenalty(PDCommon::Core::PDContext &ctx) override;
};

// =========================================================================
// Zhang 方法：各向异性惩罚 (KT = K⁻¹ · D · K⁻¹ 二次型)
// Z = ω² · ξᵀ · KT · ξ,  G_Zhang = Z * Vj * vv_j
// =========================================================================
class MechanicalZhangStabilizer : public Stabilizer {
public:
  void applyPenalty(PDCommon::Core::PDContext &ctx) override;
};

} // namespace PDCommon::Kernel

#endif // PDCOMMON_KERNEL_MECHANICAL_STABILIZERS_H
