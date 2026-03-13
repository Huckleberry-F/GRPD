#ifndef GRPD_FIELD_FIELD_H
#define GRPD_FIELD_FIELD_H

// ============================================================================
// Field.h - 物理场基类 (原 FieldBase)
// 责任：
// 1. 定义所有物理场（标量场、矢量场、张量场）的通用接口
// 2. 作为 FieldManager 多态管理的根基类
// 架构规约：对齐 Material.h / BC.h 的基类设计模式
// ============================================================================

#include <cstddef>
#include <string>

namespace GRPD::Field {

class Field {
public:
  /// @brief 构造函数
  /// @param name 物理场名称（如 "Temperature", "Displacement"）
  /// @param dim  每个粒子的分量数（标量=1, 矢量=3, 对称张量=6）
  explicit Field(const std::string &name, int dim = 1);

  virtual ~Field() = default;

  // 禁用拷贝，允许移动
  Field(const Field &) = delete;
  Field &operator=(const Field &) = delete;
  Field(Field &&) = default;
  Field &operator=(Field &&) = default;

  // -----------------------------------------------------------------------
  // 通用属性访问
  // -----------------------------------------------------------------------

  /// @brief 获取物理场名称
  const std::string &getName() const;

  /// @brief 获取每个粒子的分量维度（标量=1, 矢量=3, 张量=9）
  int getDim() const;

  // -----------------------------------------------------------------------
  // 纯虚接口：由模板子类 TypedField<T> 实现
  // -----------------------------------------------------------------------

  /// @brief 按粒子总数分配/重新分配内存，初始化为零值
  /// @param numParticles 粒子总数
  virtual void resize(size_t numParticles) = 0;

  /// @brief 当前已分配的粒子数量
  virtual size_t size() const = 0;

  /// @brief 将所有数据清零（每个时间步开始时重置变化率等场量）
  virtual void clearToZero() = 0;

  /// @brief 获取底层连续内存的 double 裸指针（用于 VTK 输出 / OpenMP 并行）
  virtual double *rawPtr() = 0;
  virtual const double *rawPtr() const = 0;

protected:
  std::string name_; // 物理场名称
  int dim_;          // 每个粒子的分量维度
};

} // namespace GRPD::Field

#endif // GRPD_FIELD_FIELD_H
