#ifndef PDCOMMON_KERNEL_THERMAL_STABILIZERS_H
#define PDCOMMON_KERNEL_THERMAL_STABILIZERS_H

#include "Stabilizer.h"
#include <vector>

namespace PDCommon::Kernel {

class SillingStabilizer : public Stabilizer {
public:
  SillingStabilizer() { name_ = "Thermal_Silling"; }
  void preCompute(PDCommon::Core::PDContext &ctx) override;
  void applyPenalty(PDCommon::Core::PDContext &ctx) override;
private:
  std::vector<double> kArr_;
};

class WanStabilizer : public Stabilizer {
public:
  WanStabilizer() { name_ = "Thermal_Wan"; }
  void preCompute(PDCommon::Core::PDContext &ctx) override;
  void applyPenalty(PDCommon::Core::PDContext &ctx) override;
private:
  std::vector<double> GWanArr_; // Precomputed scalar penalty
};

class ZhangStabilizer : public Stabilizer {
public:
  ZhangStabilizer() { name_ = "Thermal_Zhang"; }
  void preCompute(PDCommon::Core::PDContext &ctx) override;
  void applyPenalty(PDCommon::Core::PDContext &ctx) override;
private:
  std::vector<double> kArr_;
};

} // namespace PDCommon::Kernel

#endif // PDCOMMON_KERNEL_THERMAL_STABILIZERS_H
