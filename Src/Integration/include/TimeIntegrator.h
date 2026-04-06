// ============================================================================
// TimeIntegrator.h - 时间推进策略抽象基类 (Layer 1)
//
// 职责：
//   定义所有时间推进算法（显式 Euler、隐式 Newton、ADR 等）的统一接口。
//   PDSolver 通过此接口驱动时间循环，实现算法层与 PD 理论层的解耦。
//
// 架构位置：
//   PDSolver::Solve()
//     → integrator_->run()          ← L1: 本类
//         → kernel_->computeForceState()  ← L2: PDKernel
//             → mat->evaluate()     ← L3: Material
// ============================================================================

#ifndef SRC_SOLVE_PD_TIME_INTEGRATOR_H
#define SRC_SOLVE_PD_TIME_INTEGRATOR_H

#include "PDContext.h"
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace PDCommon::Kernel { class PDKernel; }

namespace Src::Integration {

// 前置声明
using PDCommon::Kernel::PDKernel;

#include <yaml-cpp/yaml.h>

/// @brief 时间推进策略抽象基类
class TimeIntegrator {
public:
  virtual ~TimeIntegrator() = default;

  // 禁用拷贝
  TimeIntegrator(const TimeIntegrator &) = delete;
  TimeIntegrator &operator=(const TimeIntegrator &) = delete;

  // -----------------------------------------------------------------------
  // 核心接口
  // -----------------------------------------------------------------------

  /// @brief 从 YAML 读取控制参数（子类可 override 追加私有参数）
  virtual void configure(const YAML::Node &solverNode);

  /// @brief 执行完整的时间推进循环（多核协同版本）
  /// @param ctx            PD 仿真上下文
  /// @param kernels        PD 积分核心集合 (L2)，支持多场多核协同推进
  /// @param outputCallback 输出回调（由 PDSolver 提供）
  virtual void run(PDCommon::Core::PDContext &ctx,
                   std::vector<std::unique_ptr<PDKernel>> &kernels,
                   std::function<void(int, double)> outputCallback) = 0;

  /// @brief 获取算法名称（如 "ExplicitEuler", "ADR"）
  virtual std::string getName() const = 0;

protected:
  struct LoadStepConfig {
    int stepId = 0;
    int numSubsteps = 1;     // 用于准静态步（如 ADR）定义细分
    double targetTime = 0.0; // 显式算法判定本段结束的目标物理时间
    double userDt = 0.0;     // 显式算法特定覆盖的步长 (0.0 表示自动计算)
  };

  std::vector<LoadStepConfig> loadStepConfigs_;
  int defaultNumLoadSteps_ = 10;
  int defaultNumSubsteps_ = 1;
  double defaultEndTime_ = 1.0;
  struct FirstOrderTarget {
    std::string primaryName, rateName;
    double *primaryPtr;
    double *ratePtr;
    size_t totalComponents;
    int dimension;
  };

  struct SecondOrderTarget {
    std::string uName, vName, aName;
    double *uPtr;
    double *vPtr;
    double *aPtr;
    size_t totalComponents;
    int dimension;
  };

  double dt_ = 1.0;
  double totalTime_ = 100.0;
  int outputInterval_ = 10;
  bool autoCalcDt_ = true; ///< 是否自动计算时间步（用户设 TimeStep_dt 后关闭）

  /// @brief 通用算力流程：清零率场 → 施加 Neumann 源项 → 计算内力
  /// @param ctx                PD 仿真上下文
  /// @param kernels            PD 积分核心集合
  /// @param rateFieldsToClear  需要清零的率场名称列表（一阶=TempRate, 二阶=Acceleration）
  void evaluateForces(PDCommon::Core::PDContext &ctx,
                      std::vector<std::unique_ptr<PDKernel>> &kernels,
                      const std::vector<std::string> &rateFieldsToClear);

  /// @brief 从 Kernel 的注册列表中自动解算和匹配二阶 ODE 系统（如 U -> V -> A）
  /// @param kernels             所有算子内核集合
  /// @param ctx                 用来查询实际场指针的上下文
  /// @param out_soTargets       [Output] 匹配出的二阶组
  /// @param out_accFieldNames   [Output] 需要被定期清零和重置的高阶率场（通常是加速度）
  void extractSecondOrderTargets(const std::vector<std::unique_ptr<PDKernel>> &kernels,
                                 PDCommon::Core::PDContext &ctx,
                                 std::vector<SecondOrderTarget> &out_soTargets,
                                 std::vector<std::string> &out_accFieldNames);

  /// @brief 从 Kernel 注册列表中提取一阶系统依赖 (如 T -> T_rate)
  void extractFirstOrderTargets(const std::vector<std::unique_ptr<PDKernel>> &kernels,
                                PDCommon::Core::PDContext &ctx,
                                std::vector<FirstOrderTarget> &out_foTargets,
                                std::vector<std::string> &out_rateFieldNames);

  /// @brief 基于 CFL 条件自动计算时间步长（默认按机械波速计算，子类可按需重写）
  /// @param ctx            PD 仿真上下文
  /// @param massScale      质量缩放因子（动力学=1.0，ADR=用户设定值）
  /// @param safetyFactor   CFL 安全系数（默认 0.8）
  virtual void computeCFLTimestep(PDCommon::Core::PDContext &ctx,
                                  double massScale = 1.0,
                                  double safetyFactor = 0.8);

  TimeIntegrator() = default; // 只允许子类构造
};

} // namespace Src::Integration

#endif // SRC_SOLVE_PD_TIME_INTEGRATOR_H
