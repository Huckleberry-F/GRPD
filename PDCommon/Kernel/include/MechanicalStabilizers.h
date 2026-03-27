#ifndef PDCOMMON_KERNEL_MECHANICAL_STABILIZERS_H
#define PDCOMMON_KERNEL_MECHANICAL_STABILIZERS_H

#include "Stabilizer.h"
#include <vector>

namespace PDCommon::Kernel {

class MechanicalSillingStabilizer : public Stabilizer {
public:
  MechanicalSillingStabilizer() { name_ = "Mechanical_Silling"; }
  void preCompute(PDCommon::Core::PDContext &ctx) override;
  void applyPenalty(PDCommon::Core::PDContext &ctx) override;

private:
  std::vector<double> bulkArr_;
  std::vector<double> rhoArr_;
};

class MechanicalWanStabilizer : public Stabilizer {
public:
  MechanicalWanStabilizer() { name_ = "Mechanical_Wan"; }
  void preCompute(PDCommon::Core::PDContext &ctx) override;
  void applyPenalty(PDCommon::Core::PDContext &ctx) override;

private:
  std::vector<double> lambdaArr_;
  std::vector<double> muArr_;
  std::vector<double> rhoArr_;
  std::vector<double> shapeAi_; // 预分配的外接张量缓存
};

class MechanicalZhangStabilizer : public Stabilizer {
public:
  MechanicalZhangStabilizer() { name_ = "Mechanical_Zhang"; }
  void preCompute(PDCommon::Core::PDContext &ctx) override;
  void applyPenalty(PDCommon::Core::PDContext &ctx) override;

private:
  std::vector<double> lambdaArr_;
  std::vector<double> muArr_;
  std::vector<double> rhoArr_;
};

} // namespace PDCommon::Kernel

#endif // PDCOMMON_KERNEL_MECHANICAL_STABILIZERS_H
