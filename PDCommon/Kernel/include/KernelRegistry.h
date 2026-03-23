// ============================================================================
// KernelRegistry.h - PD 积分核心全局注册中心 (单例)
// 责任：
// 1. 管理 PDKernel 派生类的工厂函数
// 2. 通过字符串（如 "NOSB_Thermal"）返回对应的核心实例
// 架构对标：完全对称于 MaterialRegistry / PhysicsFieldRegistry
// ============================================================================

#ifndef PDCOMMON_KERNEL_KERNEL_REGISTRY_H
#define PDCOMMON_KERNEL_KERNEL_REGISTRY_H

#include "PDKernel.h"
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace PDCommon::Kernel {

typedef std::function<std::unique_ptr<PDKernel>()> KernelCreatorFunc;

class KernelRegistry {
public:
    /// @brief 获取单例
    static KernelRegistry &getInstance();

    // 禁用拷贝
    KernelRegistry(const KernelRegistry &) = delete;
    KernelRegistry &operator=(const KernelRegistry &) = delete;

    /// @brief 注册内核类型
    /// @param type 类型名（如 "NOSB_Thermal", "BB_Elastic"）
    /// @param creator 工厂函数
    void registerType(const std::string &type, KernelCreatorFunc creator);

    /// @brief 创建内核实例
    /// @param type 已注册的类型名
    /// @return 内核实例的 unique_ptr，失败返回 nullptr
    std::unique_ptr<PDKernel> create(const std::string &type);

    /// @brief 检查是否已注册某类型
    bool hasType(const std::string &type) const;

    /// @brief 获取所有已注册的类型名列表
    std::vector<std::string> getRegisteredTypes() const;

private:
    KernelRegistry() = default;
    ~KernelRegistry() = default;

    std::map<std::string, KernelCreatorFunc> registry_;
};

// ---------------------------------------------------------------------------
// 自动注册宏
// 用法 (在 .cpp 文件末尾):
//   REGISTER_KERNEL(NOSB_Thermal, NOSB_T)
// ---------------------------------------------------------------------------
#define REGISTER_KERNEL(TypeName, Class)                                        \
    namespace {                                                                \
    struct KernelRegistrar_##TypeName {                                         \
        KernelRegistrar_##TypeName() {                                         \
            PDCommon::Kernel::KernelRegistry::getInstance().registerType(       \
                #TypeName, []() { return std::make_unique<Class>(); });         \
        }                                                                      \
    };                                                                         \
    static KernelRegistrar_##TypeName global_KernelRegistrar_##TypeName;        \
    }

} // namespace PDCommon::Kernel

#endif // PDCOMMON_KERNEL_KERNEL_REGISTRY_H
