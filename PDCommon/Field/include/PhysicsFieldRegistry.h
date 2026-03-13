#ifndef GRPD_FIELD_PHYSICS_FIELD_REGISTRY_H
#define GRPD_FIELD_PHYSICS_FIELD_REGISTRY_H

// ============================================================================
// PhysicsFieldRegistry.h - 物理场配置器注册中心 (单例)
// 责任：
// 1. 管理 PhysicsFields 派生类的工厂函数
// 2. 通过字符串（如 "Thermal"）返回对应的配置器实例
// 架构对标：完全对称于 MaterialRegistry / BCRegistry
// ============================================================================

#include "PhysicsFields.h"
#include <functional>
#include <map>
#include <memory>
#include <string>

namespace GRPD::Field {

typedef std::function<std::unique_ptr<PhysicsFields>()> PhysicsFieldsCreatorFunc;

class PhysicsFieldRegistry {
public:
    /// @brief 获取单例
    static PhysicsFieldRegistry &getInstance();

    // 禁用拷贝
    PhysicsFieldRegistry(const PhysicsFieldRegistry &) = delete;
    PhysicsFieldRegistry &operator=(const PhysicsFieldRegistry &) = delete;

    /// @brief 注册物理场配置器类型
    /// @param type 类型名（如 "Thermal", "Mechanical"）
    /// @param creator 工厂函数
    void registerType(const std::string &type, PhysicsFieldsCreatorFunc creator);

    /// @brief 创建物理场配置器实例
    /// @param type 已注册的类型名
    /// @return 配置器实例的 unique_ptr
    std::unique_ptr<PhysicsFields> create(const std::string &type);

    /// @brief 检查是否已注册某类型
    bool hasType(const std::string &type) const;

private:
    PhysicsFieldRegistry() = default;
    ~PhysicsFieldRegistry() = default;

    std::map<std::string, PhysicsFieldsCreatorFunc> registry_;
};

// ---------------------------------------------------------------------------
// 自动注册宏
// 用法 (在 .cpp 文件中):
//   REGISTER_PHYSICS_FIELDS(Thermal, ThermalFields)
// ---------------------------------------------------------------------------
#define REGISTER_PHYSICS_FIELDS(Type, Class)                                    \
    namespace {                                                                \
    struct PhysicsFieldsRegistrar_##Type {                                      \
        PhysicsFieldsRegistrar_##Type() {                                      \
            GRPD::Field::PhysicsFieldRegistry::getInstance().registerType(      \
                #Type, []() { return std::make_unique<Class>(); });             \
        }                                                                      \
    };                                                                         \
    static PhysicsFieldsRegistrar_##Type global_PhysicsFieldsRegistrar_##Type;  \
    }

} // namespace GRPD::Field

#endif // GRPD_FIELD_PHYSICS_FIELD_REGISTRY_H
