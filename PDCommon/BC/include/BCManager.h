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
    /// @param step 隶属的载荷步(0为全局)
    void addBC(std::unique_ptr<BC> bc, int step = 0);

    /// @brief 批量施加所有边界条件
    void applyAll(int currentStep = 0);

    /// @brief 仅施加源项型边界条件（如热流、对流）
    void applySources(int currentStep = 0);

    /// @brief 仅施加约束型边界条件（如固定温度）
    void applyConstraints(int currentStep = 0);

    /// @brief 按比例施加约束型边界条件（通用：根据 KBC 和 local step load factor）
    /// @param loadFactor 局部载荷系数或阶跃指示器 [0.0, 1.0]
    void applyConstraints(double loadFactor, int currentStep = 0);

    /// @brief 宣告一个 LoadStep 结束，同步更新所有的边界条件基准点
    void commitEndStep();

    /// @brief 获取当前管理的边界条件总数
    size_t size() const;

    /// @brief 清空所有边界条件
    void clear();

private:
    struct BCEntry {
        int step;
        std::unique_ptr<BC> bc;
    };
    
    // 统一容器，存储附带 Step ID 的 BC
    std::vector<BCEntry> bcs_;
};

} // namespace PDCommon::BC

#endif // PDCOMMON_BC_BC_MANAGER_H
