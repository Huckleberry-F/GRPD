#ifndef PDCOMMON_BC_BC_H
#define PDCOMMON_BC_BC_H

// ============================================================================
// BC.h - 边界条件 (Boundary Condition) 基类
// 责任：
// 1. 定义边界条件的通用接口 (apply, initialize)
// 2. 作为所有物理场边界条件（热、力等）的根基类
// 架构规约：对齐 Material 基类设计，支持注册工厂 + 多态调用
// ============================================================================

#include <string>
#include <vector>

// 前置声明，避免在基类头文件引入具体物理场依赖
namespace PDCommon::Field {
class FieldManager;
}

namespace PDCommon::BC {

class BC {
public:
  /// @brief 构造函数
  /// @param name 边界条件实例的唯一名称
  explicit BC(const std::string &name = "");

  virtual ~BC() = default;

  // 禁用拷贝
  BC(const BC &) = delete;
  BC &operator=(const BC &) = delete;

  /// @brief 获取边界条件名称
  const std::string &getName() const;

  /// @brief 设置名称
  void setName(const std::string &name);

  /// @brief 初始化接口：注入物理场管理器、粒子 ID 和参数值
  /// @param fieldManager 物理场管理器引用（通过它获取各物理场）
  /// @param particleId   粒子 ID
  /// @param values       参数值列表
  virtual void initialize(PDCommon::Field::FieldManager &fieldManager,
                          int particleId,
                          const std::vector<double> &values) = 0;

  /// @brief 施加边界条件的核心虚函数
  /// 每个具体的派生类将实现自己的施加逻辑
  virtual void apply() = 0;

  /// @brief 按比例施加边界条件（用于 ADR 准静态加载）
  /// @param loadFactor 加载比例因子 [0.0, 1.0]
  /// @details 默认实现直接调用无参 apply()。
  ///          Dirichlet 型子类（如 DisplacementBC）可重写此方法，
  ///          将施加值乘以 loadFactor 实现平滑升压。
  virtual void apply(double loadFactor) { apply(); }

  /// @brief 标识是否为约束型边界条件
  /// 约定：所有子类必须显式 override 此方法，禁止依赖基类默认值
  ///   - Dirichlet 型 (直接设定主场/速度/温度值)  → 返回 true
  ///   - Neumann/Source 型 (向变化率场累加贡献)   → 返回 false
  /// Constraint (true)  会在时间积分步末尾由 applyConstraints() 重新施加
  /// Source    (false) 仅在力计算前由 applySources() 调用一次
  virtual bool isConstraint() const { return false; }

protected:
  std::string name_; // 边界条件实例名称
};

} // namespace PDCommon::BC

#endif // PDCOMMON_BC_BC_H
