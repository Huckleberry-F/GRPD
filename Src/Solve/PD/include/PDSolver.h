// ============================================================================
// PDSolver.h - 近场动力学 (PD) 求解器
// 继承 Src::Engine::Engine，封装完整的 PD 仿真生命周期
// ============================================================================

#ifndef SRC_SOLVE_PD_SOLVER_H
#define SRC_SOLVE_PD_SOLVER_H

#include "Engine.h"
#include "PDContext.h"

namespace Src::Solve {

/// @brief PD（近场动力学）求解器
/// @details 封装了 PD 仿真的完整流程：模型生成、材料分配、物理场注册、
///          边界条件加载、时间步求解与结果输出。
///          通过 REGISTER_ENGINE_TYPE 宏自动注册到 EngineRegistry。
class PDSolver : public Src::Engine::Engine {
public:
  PDSolver();
  ~PDSolver() override = default;

  // -----------------------------------------------------------------------
  // Engine 接口实现
  // -----------------------------------------------------------------------

  /// @brief 初始化 PD 仿真上下文（模型、材料、场、边界条件）
  void Initialize(const std::string &yamlPath) override;

  /// @brief 执行 PD 求解主循环
  void Solve() override;

  /// @brief 导出 PD 计算结果
  void Output() override;

  /// @brief 返回求解器类型名
  std::string getName() const override { return "PD"; }

private:
  // -----------------------------------------------------------------------
  // PD 分步初始化私有函数（从 SE_*.cpp 迁移而来）
  // -----------------------------------------------------------------------
  void InitModel();      ///< 模型几何与粒子生成
  void InitMaterial();   ///< 材料分配与初始化
  void InitFields();     ///< 物理场与状态变量注册
  void InitConditions(); ///< 边界条件与载荷加载
  void InitNeighbors();  ///< 邻域搜索与 NeighborCount 场构建

  // -----------------------------------------------------------------------
  // PD 仿真上下文与配置
  // -----------------------------------------------------------------------
  PDCommon::Core::PDContext pdContext_; ///< PD 数据容器
  std::string yamlPath_;               ///< YAML 配置文件路径
};

} // namespace Src::Solve

#endif // SRC_SOLVE_PD_SOLVER_H
