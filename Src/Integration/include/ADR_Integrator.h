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
#include <string>
#include <vector>

namespace PDCommon::Kernel {
class PDKernel;
}

namespace Src::Integration {

using PDCommon::Kernel::PDKernel;

/// @brief 自适应动态弛豫准静态积分器
///
/// 支持两种阻尼策略（通过 YAML DampingMethod 切换）：
///   - "Viscous"      : Underwood 全局粘性自适应阻尼（默认）
///   - "LocalKinetic"  : 局部动能阻尼法（逐粒子功率判定速度清零）
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
  // numLoadSteps_, numSubsteps_, loadStepConfigs_, kbc_ 已移至 TimeIntegrator 基类
  int maxPseudoSteps_ = 5000; ///< 每个 Substep 内允许的最大迭代数
  double kineticTol_ = 1.0e-5; ///< 动能耗散收敛阈值 TOL1 (替代位移阈值)
  double massScaleFactor_ = 1.0e5; ///< 质量缩放因子（默认放大 10^5 倍）
  double rampWaveRatio_ = 2.0; ///< 爬坡期覆盖特征波长倍数（默认2倍模型长度）
  int rampItersOverride_ = 0;  ///< 手动指定爬坡步数（0=自动物理计算）

  double minRefForce_ = 1.0e-6; ///< 最低参考力截断值 (MINREF)，用于相对力判定兜底
  double fIntPrevSubstep_ = 0.0; ///< 上一子步收敛时的全场总内力范数（用于外循环增量基准）

  // -----------------------------------------------------------------------
  // ADR 初始刚度法外循环参数（非线性本构交错修正）
  // -----------------------------------------------------------------------
  double NRForceTol_ = 5.0e-3;  ///< 外循环宏观力残差收敛容差（ANSYS标准 0.5%）
  double NRDispTol_ = 5.0e-2;  ///< 外循环宏观位移残差收敛容差（ANSYS标准 5%）
  int maxNRIters_ = 10;        ///< 最大外循环迭代次数（默认 10）
  bool NRStateFrozen_ = true;  ///< 是否在外循环期间冻结非线性状态（初始刚度法），默认 true
  std::string NRStiffnessType_ = "Initial"; ///< 冻结态刚度类型："Initial" (始终 stateMode=1) 或 "Tangent" (迭代激活 stateMode=2)

  // -----------------------------------------------------------------------
  // Anderson Acceleration (AA) 参数与历史缓存
  // -----------------------------------------------------------------------
  bool useAndersonAcceleration_ = true; ///< 是否开启安德森加速
  int aaDepth_ = 3;                     ///< AA 历史深度 m
  struct AATargetHistory {
    std::vector<std::vector<double>> disp; // [history_idx][component_idx]
    std::vector<std::vector<double>> res;  // [history_idx][component_idx]
  };
  std::vector<AATargetHistory> aaHistories_; ///< 每个 Target 的 AA 历史
  std::vector<double> aaResidualNormHistory_; ///< 记录每次 AA 前的残差范数，用于重启安全阀

  /// @brief 阻尼策略："Viscous"=Underwood粘性阻尼, "LocalKinetic"=局部动能阻尼
  std::string dampingMethod_ = "Viscous";

  /// @brief 断裂评估策略："Staggered"=外部交错循环准静力严格断裂, "FastInnerLoop"=内部混合虚时极速发散断裂
  std::string fractureStrategy_ = "Staggered";

  /// @brief 内部辅助：结合 CFL 计算物理波传播所需的最小平滑爬坡步数
  int computeRampIters(PDCommon::Core::PDContext &ctx);

  // --- 内部重构：抽象出的算法推演流水线部件 ---

  std::vector<SecondOrderTarget> soTargets_;
  std::vector<std::string> accFieldNames_;
  std::vector<std::vector<double>> velHalfOld_;
  std::vector<std::vector<double>> aOld_;
  std::vector<std::vector<double>> dispOld_;
  std::vector<std::vector<double>> dispBase_;
  std::vector<std::vector<double>> dispNROld_; ///< 外循环位移快照
  std::vector<std::vector<double>> forceCorr_; ///< NR 外循环本构修正力（加速度单位），= a_true - a_frozen
  std::vector<double> nodalMass_; ///< 各粒子的真实物理质量
  bool isFirstExplicitTick_ = true;
  double kineticRatio_ = 0.0;
  double dispRatio_ = 0.0;
  double forceRatio_ = 0.0;
  double maxKineticEnergy_ = 0.0; ///< 记录内循环的历史最大动能（L2范数）
  double lastValidCn_ = 0.0;

  void initializeHistoryVariables();
  void initializeNodalMass(PDCommon::Core::PDContext &ctx);
  void saveBaseDisplacement();
  void saveOldDisplacement();
  void saveNROldDisplacement();  ///< 保存外循环开始时的位移
  double computeAdaptiveDamping(double dt);
  void updateKinematicsLeapfrog(double cn, double dt);
  void computeConvergenceCriteria(double currentFRef, const double* damage = nullptr);

  /// @brief 计算外循环宏观收敛准则（ANSYS风格）
  /// @param[out] macroForceRatio 宏观力残差比 = ||R_free|| / max(||F_int_all||, MINREF)
  /// @param[out] macroDispRatio  宏观位移残差比 = ||du_NR|| / ||du_substep||
  void computeNRConvergence(double fIntTotal, double &macroForceRatio, double &macroDispRatio, const double* damage = nullptr);
};

} // namespace Src::Integration

#endif // SRC_INTEGRATION_ADR_INTEGRATOR_H
