#ifndef GRPD_FIELD_TYPED_FIELD_H
#define GRPD_FIELD_TYPED_FIELD_H

// ============================================================================
// TypedField.h - 模板物理场类 (原 Field<T>，纯头文件)
// 责任：
// 1. 继承 Field 基类，提供类型安全的 SoA 连续内存存储
// 2. 支持标量场 (dim=1)、矢量场 (dim=3)、张量场 (dim=6) 等
// 架构规约：对标 ThermalBC / ThermalMat 的派生类角色
// ============================================================================

#include "Field.h"
#include <algorithm>
#include <vector>

namespace GRPD::Field {

/// @brief 模板物理场类
/// @tparam T 数据类型，默认为 double（支持 int 等扩展）
template <typename T = double>
class TypedField : public Field {
public:
    /// @brief 构造函数
    /// @param name 物理场名称（如 "Temperature", "Displacement"）
    /// @param dim  每个粒子的分量数（标量=1, 矢量=3）
    explicit TypedField(const std::string &name, int dim = 1)
        : Field(name, dim) {}

    ~TypedField() override = default;

    // 禁用拷贝，允许移动
    TypedField(const TypedField &) = delete;
    TypedField &operator=(const TypedField &) = delete;
    TypedField(TypedField &&) = default;
    TypedField &operator=(TypedField &&) = default;

    // -----------------------------------------------------------------------
    // 覆盖 Field 纯虚接口
    // -----------------------------------------------------------------------

    /// @brief 按粒子总数分配内存，初始化为零值
    /// 实际分配长度 = numParticles * dim_
    void resize(size_t numParticles) override {
        data_.assign(numParticles * static_cast<size_t>(dim_), T{});
    }

    /// @brief 当前存储的粒子数量
    size_t size() const override {
        return dim_ > 0 ? data_.size() / static_cast<size_t>(dim_) : 0;
    }

    /// @brief 将所有数据清零
    void clearToZero() override {
        std::fill(data_.begin(), data_.end(), T{});
    }

    /// @brief 获取底层 double* 裸指针（用于 VTK 输出等通用接口）
    double *rawPtr() override {
        return reinterpret_cast<double *>(data_.data());
    }

    /// @brief 获取底层 const double* 裸指针
    const double *rawPtr() const override {
        return reinterpret_cast<const double *>(data_.data());
    }

    // -----------------------------------------------------------------------
    // 类型安全的数据访问接口
    // -----------------------------------------------------------------------

    /// @brief 获取指定粒子、指定分量的值
    /// @param id   粒子 ID（全局索引）
    /// @param comp 分量索引（标量场为 0，矢量场为 0/1/2）
    T get(int id, int comp = 0) const {
        return data_[static_cast<size_t>(id) * dim_ + comp];
    }

    /// @brief 设置指定粒子、指定分量的值
    void set(int id, T val, int comp = 0) {
        data_[static_cast<size_t>(id) * dim_ + comp] = val;
    }

    /// @brief 累加指定粒子、指定分量的值（适用于力/变化率等可叠加量）
    void add(int id, T val, int comp = 0) {
        data_[static_cast<size_t>(id) * dim_ + comp] += val;
    }

    // -----------------------------------------------------------------------
    // 指针访问（OpenMP 并行 / 批量操作）
    // -----------------------------------------------------------------------

    /// @brief 获取类型安全的数据裸指针
    T *dataPtr() { return data_.data(); }

    /// @brief 获取类型安全的 const 数据裸指针
    const T *dataPtr() const { return data_.data(); }

    /// @brief 获取底层 std::vector 引用（高级用途）
    std::vector<T> &getData() { return data_; }
    const std::vector<T> &getData() const { return data_; }

private:
    std::vector<T> data_; // SoA 连续内存存储：长度 = numParticles * dim_
};

} // namespace GRPD::Field

#endif // GRPD_FIELD_TYPED_FIELD_H
