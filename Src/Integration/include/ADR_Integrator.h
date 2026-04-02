// ============================================================================
// ADR_Integrator.h - 自适应动态弛豫准静态积分器 (Adaptive Dynamic Relaxation)
//
// 职责：
//   实现类 ANSYS 风格的 LoadStep / Substep / KBC 显式准静态时间推进架构。
//   通过自适应阻尼参数 cn 与 Leapfrog (Verlet) 时间积分格式，
//   在显式框架内实现准静态加载，使系统稳定弛豫至平衡构型。
//
// 架构位置：
//   继承自 TimeIntegrator，通过 REGISTER_TIME_INTEGRATOR 宏注入注册表。
//   YAML 中配置 TimeIntegrator: "ADR" 即可触发。
//
// 算法核心：
//   外层循环：LoadStep × Substep (载荷步 × 增量子步)
//     - KBC=0 (Ramp)：loadFactor 在子步间线性插值
//     - KBC=1 (Step)：loadFactor 在载荷步开始时阶跃至目标值
//   内层循环：伪时间迭代 (Pseudo Steps)
//     1. 计算内力 → 加速度 a_new
//     2. 自适应阻尼 cn = 2*sqrt(cn1/cn2)，上限 1.8/dt
//     3. Leapfrog 速度-位移更新（含阻尼衰减项）
//     4. 收敛判定：位移增量范数 TOL2 < dispTol 且力残差范数 TOL3 < forceTol
// ============================================================================

#ifndef SRC_INTEGRATION_ADR_INTEGRATOR_H
#define SRC_INTEGRATION_ADR_INTEGRATOR_H

#include "TimeIntegrator.h"
#include <memory>
#include <vector>

namespace PDCommon::Kernel {
class PDKernel;
}

namespace Src::Integration {

using PDCommon::Kernel::PDKernel;

/// @brief 自适应动态弛豫准静态积分器
class ADR_Integrator : public TimeIntegrator {
public:
  ADR_Integrator() = default;
  ~ADR_Integrator() override = default;

  /// @brief 从 YAML 读取 ADR 专属控制参数
  void configure(const YAML::Node &solverNode) override;

  /// @brief 执行 LoadStep -> Smooth Ramp -> Hold & Converge 主循环
  void run(PDCommon::Core::PDContext &ctx,
           std::vector<std::unique_ptr<PDKernel>> &kernels,
           std::function<void(int, double)> outputCallback) override;

  std::string getName() const override { return "ADR"; }

private:
  // -----------------------------------------------------------------------
  // ADR 专属参数（从 YAML Solver 段读取）
  // -----------------------------------------------------------------------
  int numLoadSteps_ = 10;        ///< 物理载荷大步数量
  int numSubsteps_ = 1;          ///< 每个载荷步细分为几个增量子步
  int kbc_ = 0;                  ///< 加载控制：0=Ramp(坡道加载), 1=Step(阶跃突加)
  int maxPseudoSteps_ = 5000;    ///< 每个 Substep 内允许的最大迭代数
  double dispTol_ = 1.0e-6;      ///< 位移收敛阈值 TOL2
  double forceTol_ = 1.0e-4;     ///< 力平衡收敛阈值 TOL3
  double massScaleFactor_ = 1.0e5; ///< 质量缩放因子（默认放大 10^5 倍）
  double rampWaveRatio_ = 2.0;     ///< 爬坡期覆盖特征波长倍数（默认2倍模型长度）
  int rampItersOverride_ = 0;      ///< 手动指定爬坡步数（0=自动物理计算）

  /// @brief 内部辅助：结合 CFL 计算物理波传播所需的最小平滑爬坡步数
  int computeRampIters(PDCommon::Core::PDContext &ctx);

  // --- 内部重构：抽象出的算法推演流水线部件 ---

  std::vector<SecondOrderTarget> soTargets_;
  std::vector<std::string> accFieldNames_;
  std::vector<std::vector<double>> velHalfOld_;
  std::vector<std::vector<double>> aOld_;
  std::vector<std::vector<double>> dispOld_;
  std::vector<std::vector<double>> dispBase_;
  bool isFirstExplicitTick_ = true;
  double TOL1_ = 0.0;
  double TOL2_ = 0.0;
  double TOL3_ = 0.0;

  void initializeHistoryVariables();
  void saveBaseDisplacement();
  void saveOldDisplacement();
  double computeAdaptiveDamping(double dt);
  void updateKinematicsLeapfrog(double cn, double dt);
  void computeConvergenceCriteria();
};

} // namespace Src::Integration

#endif // SRC_INTEGRATION_ADR_INTEGRATOR_H
