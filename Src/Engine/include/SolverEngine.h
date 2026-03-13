#ifndef SOLVERENGINE_H
#define SOLVERENGINE_H

// ============================================================================
// SolverEngine.h - 仿真引擎主控类
// 责任：
// 1. 作为整个控制台应用程序 (Src) 的最高级生命周期管家
// 2. 调度底层物理世界的初始化、求解大循环和结果输出
// 3. 未来扩展 FEM 等其他物理场时，将在这里进行模型组装和耦合调度
// ============================================================================

#include "PDSimulater.h"

namespace GRPD::Engine {

class SolverEngine {
public:
  SolverEngine();
  ~SolverEngine() = default;

  // 禁用拷贝
  SolverEngine(const SolverEngine &) = delete;
  SolverEngine &operator=(const SolverEngine &) = delete;

  /// @brief 初始化整个仿真工程（解析参数、生成网格、分配材料等）
  void Initialize(const std::string &yamlPath = "../../Input/PD.yaml");

  /// @brief 启动主求解循环
  void Solve();

  /// @brief 将当前步的结果导出到文件
  void Output();

  // -------------------------------------------------------------
  // 【未来扩展预留接口示例】
  // void InitializeFEM(const std::string& femInpPath);
  // void CoupledSolve(); // 执行 PD-FEM 耦合时间积分
  // -------------------------------------------------------------

private:
  // 分步初始化辅助函数
  void InitModelPD();
  void InitMaterialPD();
  void InitFieldsPD();      // 物理场与状态变量注册
  void InitConditionsPD();  // 载荷与边界条件读取

  // PDE求解和输出的内部具体实现
  void SolvePD();
  void OutputPD();

  GRPD::Core::PDSimulater pdContext_;

  // 【未来扩展】
  // std::unique_ptr<GRPD::Core::FEMSimulater> femContext_;
};

} // namespace GRPD::Engine

#endif // SOLVERENGINE_H
