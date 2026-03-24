// ============================================================================
// Engine.h - 求解器引擎抽象基类
// 定义所有求解器的统一接口，任何具体求解器（PD、FEM 等）均需继承此类
// ============================================================================

#ifndef SRC_ENGINE_ENGINE_H
#define SRC_ENGINE_ENGINE_H

#include <string>

namespace Src::Engine {

/// @brief 求解器引擎抽象基类
/// @details 定义统一的初始化 → 求解 → 输出三阶段生命周期接口。
///          具体求解器（如 PDSolver、FEMSolver）继承此基类，
///          通过 EngineRegistry 注册，由 EngineManager 动态调度。
class Engine {
public:
  Engine() = default;
  virtual ~Engine();

  /// @brief 初始化求解器（解析配置、生成网格、分配材料与场等）
  /// @param yamlPath YAML 配置文件路径
  virtual void Initialize(const std::string &yamlPath) = 0;

  /// @brief 执行主求解循环
  virtual void Solve() = 0;

  /// @brief 导出计算结果
  virtual void Output() = 0;

  /// @brief 获取求解器类型名称（如 "PD", "FEM"）
  virtual std::string getName() const = 0;

  /// @brief 打印该引擎使用的注册表信息（可选覆盖）
  /// @details 由 EngineManager 在 Setup 阶段调用，各子类自行决定打印内容
  virtual void printRegistrySummary() const {}
};

} // namespace Src::Engine

#endif // SRC_ENGINE_ENGINE_H
