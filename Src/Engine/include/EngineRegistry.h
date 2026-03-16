// ============================================================================
// EngineRegistry.h - 求解器全局注册中心（单例）
// 责任：
// 1. 保存所有编译期静态注入的求解器工厂函数
// 2. 提供统一的接口用于通过类型名称字符串创建求解器实例
// 架构对标：完全对称于 MaterialRegistry / BCRegistry / FieldRegistry
// ============================================================================

#ifndef SRC_ENGINE_ENGINE_REGISTRY_H
#define SRC_ENGINE_ENGINE_REGISTRY_H

#include "Engine.h"
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace Src::Engine {

// ---------------------------------------------------------------------------
// 工厂函数类型：无参，返回一个具体 Engine 的 unique_ptr
// ---------------------------------------------------------------------------
using EngineCreatorFunc = std::function<std::unique_ptr<Engine>()>;

class EngineRegistry {
public:
  // -----------------------------------------------------------------------
  // 单例模式访问接口
  // -----------------------------------------------------------------------
  static EngineRegistry &getInstance();

  // 禁用拷贝
  EngineRegistry(const EngineRegistry &) = delete;
  EngineRegistry &operator=(const EngineRegistry &) = delete;

  // -----------------------------------------------------------------------
  // 工厂注册机制
  // -----------------------------------------------------------------------

  /// @brief 注册一个求解器类型及其工厂函数
  /// @param type 求解器类型名（如 "PD", "FEM"）
  /// @param creator 创建该类型实例的工厂函数
  void registerType(const std::string &type, EngineCreatorFunc creator);

  /// @brief 使用注册的工厂函数创建求解器实例
  /// @param type 已注册的类型名
  /// @return 创建的 Engine 智能指针
  std::unique_ptr<Engine> create(const std::string &type);

  /// @brief 检查是否已注册某类型
  bool hasType(const std::string &type) const;

  /// @brief 获取所有已注册的类型名列表
  std::vector<std::string> getRegisteredTypes() const;

private:
  EngineRegistry() = default;
  ~EngineRegistry() = default;

  std::map<std::string, EngineCreatorFunc> registry_;
};

// ---------------------------------------------------------------------------
// 自动注册宏：在具体求解器的 .cpp 文件中使用此宏进行编译期静态注册
// 用法: REGISTER_ENGINE_TYPE(PD, [](){ return std::make_unique<PDSolver>(); })
// ---------------------------------------------------------------------------
#define REGISTER_ENGINE_TYPE(Name, Creator)                                    \
  namespace {                                                                  \
  struct EngineRegistrar_##Name {                                              \
    EngineRegistrar_##Name() {                                                 \
      Src::Engine::EngineRegistry::getInstance().registerType(#Name, Creator); \
    }                                                                          \
  };                                                                           \
  static EngineRegistrar_##Name global_EngineRegistrar_##Name;                 \
  }

} // namespace Src::Engine

#endif // SRC_ENGINE_ENGINE_REGISTRY_H
