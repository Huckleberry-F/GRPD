#ifndef PDCOMMON_FIELD_FIELD_REGISTRY_H
#define PDCOMMON_FIELD_FIELD_REGISTRY_H

// ============================================================================
// FieldRegistry.h - 物理场全局注册中心 (单例)
// 责任：
// 1. 保存所有编译期静态注入的物理场工厂函数
// 2. 提供统一的接口用于通过类型名称字符串创建物理场实例
// 架构对标：完全对称于 BCRegistry / MaterialRegistry 的工厂注册模式
// ============================================================================

#include "Field.h"
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace PDCommon::Field {

// ---------------------------------------------------------------------------
// 工厂函数定义：根据名称返回一个具体 Field 的 unique_ptr
// ---------------------------------------------------------------------------
typedef std::function<std::unique_ptr<Field>(const std::string &, int)>
    FieldCreatorFunc;

class FieldRegistry {
public:
    // -----------------------------------------------------------------------
    // 单例模式访问接口
    // -----------------------------------------------------------------------
    static FieldRegistry &getInstance();

    // 禁用拷贝
    FieldRegistry(const FieldRegistry &) = delete;
    FieldRegistry &operator=(const FieldRegistry &) = delete;

    // -----------------------------------------------------------------------
    // 工厂注册机制 (Registry)
    // -----------------------------------------------------------------------

    /// @brief 注册一个物理场类型及其工厂函数
    /// @param type    物理场类型名（如 "ScalarField", "VectorField"）
    /// @param creator 创建该类型实例的工厂函数
    void registerFieldType(const std::string &type, FieldCreatorFunc creator);

    /// @brief 使用注册的工厂函数创建物理场实例
    /// @param type 已注册的类型名（如 "DoubleField", "IntField"）
    /// @param name 新实例名称
    /// @param dim  每个粒子的分量数（标量=1, 矢量=3, 张量=9）
    /// @return 创建的 Field 智能指针
    std::unique_ptr<Field> createField(const std::string &type,
                                        const std::string &name,
                                        int dim);

    /// @brief 获取所有已注册的类型名列表
    std::vector<std::string> getRegisteredTypes() const;

private:
    FieldRegistry() = default;
    ~FieldRegistry() = default;

    // 类型注册表：类型名 -> 工厂函数
    std::map<std::string, FieldCreatorFunc> registryMap_;
};

// ---------------------------------------------------------------------------
// 自动注册宏：在需要注册新场类型的 .cpp 文件中使用此宏进行编译期静态注册
// ---------------------------------------------------------------------------
#define REGISTER_FIELD_TYPE(Type, Creator)                                      \
    namespace {                                                                \
    struct FieldRegistrar_##Type {                                              \
        FieldRegistrar_##Type() {                                              \
            PDCommon::Field::FieldRegistry::getInstance().registerFieldType(       \
                #Type, Creator);                                               \
        }                                                                      \
    };                                                                         \
    static FieldRegistrar_##Type global_FieldRegistrar_##Type;                 \
    }

} // namespace PDCommon::Field

#endif // PDCOMMON_FIELD_FIELD_REGISTRY_H
