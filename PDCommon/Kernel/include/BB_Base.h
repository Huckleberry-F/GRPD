#ifndef PDCOMMON_KERNEL_BB_BASE_H
#define PDCOMMON_KERNEL_BB_BASE_H

#include "PDKernel.h"
#include <cmath>
#include <yaml-cpp/yaml.h>

namespace PDCommon::Kernel {

/// @brief 键基和常规态基近场动力学 (BBPD / OSB) 通用基类
/// 负责实现表面体积修正与微观力求导的基础功能
class BB_Base : public PDKernel {
public:
  BB_Base() = default;
  ~BB_Base() override = default;

  void configure(const YAML::Node &solverNode) override;

protected:

  /// @brief 计算并存入每个键的表面体积修正因子 (Skin Effect Correction)
  /// 在 BB 和 OSB 算法中，边界粒子的缺失刚度需要被显式补偿
  void ComputeSurfaceCorrections(PDCommon::Core::PDContext &ctx);
};

} // namespace PDCommon::Kernel

#endif // PDCOMMON_KERNEL_BB_BASE_H
