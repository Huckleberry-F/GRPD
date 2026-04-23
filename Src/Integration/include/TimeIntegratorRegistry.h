// ============================================================================
// TimeIntegratorRegistry.h - 时间积分器全局注册中心（单例）
// 责任：
// 1. 管理 TimeIntegrator 派生类的工厂函数
// 2. 通过字符串（如 "Explicit", "CentralDifference"）返回对应的积分器实例
// 架构对标：完全对称于 KernelRegistry / MaterialRegistry
// ============================================================================

#ifndef SRC_INTEGRATION_TIME_INTEGRATOR_REGISTRY_H
#define SRC_INTEGRATION_TIME_INTEGRATOR_REGISTRY_H

#include "TimeIntegrator.h"
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace Src::Integration {

// ---------------------------------------------------------------------------
// 工厂函数类型：无参，返回一个具体 TimeIntegrator 的 unique_ptr
// ---------------------------------------------------------------------------
using TimeIntegratorCreator = std::function<std::unique_ptr<TimeIntegrator>()>;

class TimeIntegratorRegistry {
public:
    /// @brief 获取单例
    static TimeIntegratorRegistry &getInstance();

    // 禁用拷贝
    TimeIntegratorRegistry(const TimeIntegratorRegistry &) = delete;
    TimeIntegratorRegistry &operator=(const TimeIntegratorRegistry &) = delete;

    /// @brief 注册积分器类型
    /// @param type 类型名（如 "Explicit", "CentralDifference", "ADR"）
    /// @param creator 工厂函数
    void registerType(const std::string &type, TimeIntegratorCreator creator);

    /// @brief 创建积分器实例
    /// @param type 已注册的类型名
    /// @return 积分器实例的 unique_ptr，失败返回 nullptr
    std::unique_ptr<TimeIntegrator> create(const std::string &type);

    /// @brief 检查是否已注册某类型
    bool hasType(const std::string &type) const;

    /// @brief 获取所有已注册的类型名列表
    std::vector<std::string> getRegisteredTypes() const;

private:
    TimeIntegratorRegistry() = default;
    ~TimeIntegratorRegistry() = default;

    std::map<std::string, TimeIntegratorCreator> registry_;
};

// ---------------------------------------------------------------------------
// 自动注册宏
// 用法 (在 .cpp 文件末尾):
//   REGISTER_TIME_INTEGRATOR(Explicit, ExplicitEuler)
// ---------------------------------------------------------------------------
#define REGISTER_TIME_INTEGRATOR(TypeName, Class)                                \
    namespace {                                                                  \
    struct TimeIntegratorRegistrar_##TypeName {                                   \
        TimeIntegratorRegistrar_##TypeName() {                                   \
            Src::Integration::TimeIntegratorRegistry::getInstance()              \
                .registerType(#TypeName,                                         \
                              []() { return std::make_unique<Class>(); });       \
        }                                                                        \
    };                                                                           \
    static TimeIntegratorRegistrar_##TypeName                                    \
        global_TimeIntegratorRegistrar_##TypeName;                               \
    }

} // namespace Src::Integration

#endif // SRC_INTEGRATION_TIME_INTEGRATOR_REGISTRY_H
