#ifndef PDCOMMON_BC_BC_MANAGER_H
#define PDCOMMON_BC_BC_MANAGER_H

// ============================================================================
// BCManager.h - 边界条件实例管理器 (非单例，每个模型一个)
// 责任：
// 1. 管理某个具体模型上下文中的所有边界条件实例
// 2. 提供统一的施加接口给求解轴
// 架构对标：完全对称于 MaterialManager（泛化容器，不依赖具体类型）
// ============================================================================

#include "BC.h"
#include <memory>
#include <vector>

namespace PDCommon::BC {

class BCManager {
public:
    BCManager() = default;
    ~BCManager() = default;

    // 禁用拷贝，允许移动
    BCManager(const BCManager&) = delete;
    BCManager& operator=(const BCManager&) = delete;
    BCManager(BCManager&&) = default;
    BCManager& operator=(BCManager&&) = default;

    // -----------------------------------------------------------------------
    // 边界条件实例管理 (泛化接口)
    // -----------------------------------------------------------------------

    /// @brief 添加一个边界条件实例（独占所有权）
    /// @param bc 边界条件实例的 unique_ptr
    void addBC(std::unique_ptr<BC> bc);

    /// @brief 批量施加所有边界条件
    void applyAll();

    /// @brief 仅施加源项型边界条件（如热流、对流）
    void applySources();

    /// @brief 仅施加约束型边界条件（如固定温度）
    void applyConstraints();

    /// @brief 获取当前管理的边界条件总数
    size_t size() const;

    /// @brief 清空所有边界条件
    void clear();

private:
    // 统一容器，通过多态调用 apply()
    std::vector<std::unique_ptr<BC>> bcs_;
};

} // namespace PDCommon::BC

#endif // PDCOMMON_BC_BC_MANAGER_H
