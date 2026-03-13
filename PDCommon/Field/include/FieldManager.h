#ifndef GRPD_FIELD_FIELD_MANAGER_H
#define GRPD_FIELD_FIELD_MANAGER_H

// ============================================================================
// FieldManager.h - 物理场管理器 (数据总线)
// 责任：
// 1. 统一持有所有已注册的物理场（标量场、矢量场、张量场）
// 2. 提供按名称注册、查询、批量操作的接口
// 架构规约：对标 BCManager / MaterialManager 的管理器模式
// ============================================================================

#include "Field.h"
#include "TypedField.h"
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace GRPD::Field {

class FieldManager {
public:
    FieldManager() = default;
    ~FieldManager() = default;

    // 禁用拷贝，允许移动
    FieldManager(const FieldManager &) = delete;
    FieldManager &operator=(const FieldManager &) = delete;
    FieldManager(FieldManager &&) = default;
    FieldManager &operator=(FieldManager &&) = default;

    // -----------------------------------------------------------------------
    // 注册接口
    // -----------------------------------------------------------------------

    /// @brief 注册一个新的物理场
    /// 如果同名场已存在，直接返回已有场的指针（幂等操作）
    /// @tparam T 数据类型（默认 double）
    /// @param name 物理场名称（如 "Temperature", "Displacement"）
    /// @param dim  每个粒子的分量数（标量=1, 矢量=3, 张量=6）
    /// @return 已注册的 TypedField<T> 指针
    template <typename T = double>
    TypedField<T> *registerField(const std::string &name, int dim = 1);

    // -----------------------------------------------------------------------
    // 查询接口
    // -----------------------------------------------------------------------

    /// @brief 获取已注册的物理场基类指针（不存在返回 nullptr）
    Field *getField(const std::string &name);
    const Field *getField(const std::string &name) const;

    /// @brief 获取带类型安全的物理场指针（不存在或类型不匹配返回 nullptr）
    /// @tparam T 数据类型
    template <typename T = double>
    TypedField<T> *getFieldAs(const std::string &name);

    /// @brief 检查某个物理场是否已注册
    bool hasField(const std::string &name) const;

    /// @brief 获取所有已注册物理场的名称列表（用于调试/VTK 输出枚举）
    std::vector<std::string> getFieldNames() const;

    /// @brief 获取已注册物理场总数
    size_t getFieldCount() const;

    // -----------------------------------------------------------------------
    // 工厂创建接口（通过 FieldRegistry 动态创建）
    // -----------------------------------------------------------------------

    /// @brief 通过已注册的类型名创建物理场并加入管理器
    /// @param typeName 已在 FieldRegistry 中注册的类型名（如 "ScalarField"）
    /// @param fieldName 新场的实例名称（如 "Temperature", "Damage"）
    /// @return 新创建的 Field 指针（管理器持有所有权），失败返回 nullptr
    Field *createField(const std::string &typeName,
                       const std::string &fieldName);

    // -----------------------------------------------------------------------
    // 批量操作
    // -----------------------------------------------------------------------

    /// @brief 对所有已注册的场统一按粒子数分配内存
    /// @param numParticles 粒子总数
    void resizeAll(size_t numParticles);

private:
    /// 物理场注册表：场名 -> unique_ptr<Field>
    std::map<std::string, std::unique_ptr<Field>> fields_;
};

// ===========================================================================
// 模板方法内联实现
// ===========================================================================

template <typename T>
TypedField<T> *FieldManager::registerField(const std::string &name, int dim) {
    // 幂等：如果同名场已存在，直接返回已有指针
    auto it = fields_.find(name);
    if (it != fields_.end()) {
        return dynamic_cast<TypedField<T> *>(it->second.get());
    }
    // 创建新的 TypedField<T> 并注册
    auto field = std::make_unique<TypedField<T>>(name, dim);
    TypedField<T> *ptr = field.get();
    fields_[name] = std::move(field);
    return ptr;
}

template <typename T>
TypedField<T> *FieldManager::getFieldAs(const std::string &name) {
    auto it = fields_.find(name);
    if (it == fields_.end()) {
        return nullptr;
    }
    return dynamic_cast<TypedField<T> *>(it->second.get());
}

} // namespace GRPD::Field

#endif // GRPD_FIELD_FIELD_MANAGER_H
