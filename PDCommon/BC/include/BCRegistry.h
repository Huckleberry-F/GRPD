#ifndef GRPD_BC_BC_REGISTRY_H
#define GRPD_BC_BC_REGISTRY_H

// ============================================================================
// BCRegistry.h - 边界条件全局注册中心 (单例)
// 责任：
// 1. 保存所有编译期静态注入的边界条件工厂函数
// 2. 提供统一的接口用于创建具体的边界条件对象
// 架构对标：完全对称于 MaterialRegistry 的工厂注册模式
// ============================================================================

#include "BC.h"
#include <functional>
#include <map>
#include <memory>
#include <string>

namespace GRPD::BC {

// ---------------------------------------------------------------------------
// 工厂函数定义：根据名称返回一个具体 BC 的 unique_ptr
// ---------------------------------------------------------------------------
typedef std::function<std::unique_ptr<BC>(const std::string &)> BCCreatorFunc;

class BCRegistry {
public:
    // -----------------------------------------------------------------------
    // 单例模式访问接口
    // -----------------------------------------------------------------------
    static BCRegistry &getInstance();

    // 禁用拷贝
    BCRegistry(const BCRegistry &) = delete;
    BCRegistry &operator=(const BCRegistry &) = delete;

    // -----------------------------------------------------------------------
    // 工厂注册机制 (Registry)
    // -----------------------------------------------------------------------

    /// @brief 注册一个边界条件类型及其工厂函数
    /// @param type 边界条件类型名（如 "ThermalBC"）
    /// @param creator 创建该类型实例的工厂函数
    void registerBCType(const std::string &type, BCCreatorFunc creator);

    /// @brief 使用注册的工厂函数创建边界条件实例
    /// @param type 已注册的类型名
    /// @param name 新实例名称
    /// @return 创建的 BC 智能指针
    std::unique_ptr<BC> createBC(const std::string &type,
                                  const std::string &name);

private:
    BCRegistry() = default;
    ~BCRegistry() = default;

    // 类型注册表：类型名 -> 工厂函数
    std::map<std::string, BCCreatorFunc> registryMap_;
};

// ---------------------------------------------------------------------------
// 自动注册宏：在派生类 .cpp 文件中使用此宏进行编译期静态注册
// ---------------------------------------------------------------------------
#define REGISTER_BC_TYPE(Type, Creator)                                        \
    namespace {                                                                \
    struct BCRegistrar_##Type {                                                 \
        BCRegistrar_##Type() {                                                 \
            GRPD::BC::BCRegistry::getInstance().registerBCType(#Type, Creator);\
        }                                                                      \
    };                                                                         \
    static BCRegistrar_##Type global_BCRegistrar_##Type;                        \
    }

} // namespace GRPD::BC

#endif // GRPD_BC_BC_REGISTRY_H
