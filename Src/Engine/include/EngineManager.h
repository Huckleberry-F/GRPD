// ============================================================================
// EngineManager.h - 仿真引擎顶层调度器
// 责任：
// 1. 解析 YAML 配置，确定需要实例化哪些求解器
// 2. 通过 EngineRegistry 动态创建求解器实例
// 3. 统一调度所有求解器的 Initialize → Solve → Output 生命周期
// 不包含任何具体物理（PD/FEM）相关的头文件或逻辑
// ============================================================================

#ifndef SRC_ENGINE_ENGINE_MANAGER_H
#define SRC_ENGINE_ENGINE_MANAGER_H

#include "Engine.h"
#include <memory>
#include <string>
#include <vector>

namespace Src::Engine {

class EngineManager {
public:
  EngineManager();
  ~EngineManager() = default;

  // 禁用拷贝
  EngineManager(const EngineManager &) = delete;
  EngineManager &operator=(const EngineManager &) = delete;

  /// @brief 启动仿真工程
  /// @details 解析 YAML 中的 Solver.Engine，通过 EngineRegistry 创建对应的
  ///          求解器实例，打印注册表信息，然后依次调用每个求解器的 Initialize()
  void Setup(const std::string &yamlPath = "../../Input/PD.yaml");

  /// @brief 启动主求解循环（遍历所有求解器）
  void RunAll();

  /// @brief 导出计算结果（遍历所有求解器）
  void ExportAll();

private:
  /// @brief 持有所有激活的求解器实例
  std::vector<std::unique_ptr<Engine>> engines_;
};

} // namespace Src::Engine

#endif // SRC_ENGINE_ENGINE_MANAGER_H
