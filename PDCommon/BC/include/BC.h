#ifndef PDCOMMON_BC_BC_H
#define PDCOMMON_BC_BC_H

// ============================================================================
// BC.h - 边界条件 (Boundary Condition) 基类
// 责任：
// 1. 定义边界条件的通用接口 (apply, initialize)
// 2. 作为所有物理场边界条件（热、力等）的根基类
// 3. 提供静态调度辅助函数（applySources, applyConstraints, commitEndStep）
//    使调用方无需手写循环，同时保持 Manager 为纯容器
// 架构规约：对齐 Material 基类设计，支持注册工厂 + 多态调用
// ============================================================================

#include <memory>
#include <string>
#include <vector>

// 前置声明，避免在基类头文件引入具体依赖。

namespace PDCommon::Field {
class FieldManager;
}

namespace PDCommon::BC {

class BC;
class BCManager;


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

  /// @brief 传入底层缩放相关的参数供高级边界条件（如压强转体力）使用
  virtual void setScalingFactors(double dx, double density, double massScale, int dim = 3, double thickness = 1.0) {}

  /// @brief 手动注入该 BC 在起始时的初始值（用于跨步载荷继承）
  virtual void setPrevVal(double val) {}

  /// @brief 施加边界条件的核心虚函数
  /// @brief 施加边界条件的核心虚函数
  /// 每个具体的派生类将实现自己的施加逻辑
  virtual void apply() = 0;

  /// @brief 按比例施加边界条件（用于随时间/历程增量加载）
  /// @param loadFactor 加载比例因子 [0.0, 1.0]
  /// @details 默认实现直接调用无参 apply()。
  virtual void apply(double loadFactor) { apply(); }

  /// @brief 宣告一个 LoadStep 结束，指示底层记录当前值作为下一步的起点（KBC 连续 Ramp 用）
  virtual void commitEndStep() {}

  /// @brief 标识是否为约束型边界条件
  /// 约定：所有子类必须显式 override 此方法，禁止依赖基类默认值
  ///   - Dirichlet 型 (直接设定主场/速度/温度值)  → 返回 true
  ///   - Neumann/Source 型 (向变化率场累加贡献)   → 返回 false
  /// Constraint (true)  会在时间积分步末尾由 applyConstraints() 重新施加
  /// Source    (false) 仅在力计算前由 applySources() 调用一次
  virtual bool isConstraint() const { return false; }

  /// @brief 获取受约束粒子 ID，如无则返回 -1
  virtual int getParticleId() const { return -1; }

  /// @brief 获取受约束轴向，如无则返回 0
  virtual int getAxis() const { return 0; }

  /// @brief 设置用于查表的 Table 名称。
  /// @param names 各维度对应的 Table 名称（如为空或 "None" 表示不用 Table）
  virtual void setTableNames(const std::vector<std::string> &names) {}

  /// @brief 传入当前时间/进度，执行带 Table 查表的施加逻辑。默认退化为 apply()
  virtual void applyWithTime(double currentTime) { apply(1.0); }

  // -----------------------------------------------------------------------
  // 静态调度辅助函数（供 TimeIntegrator 等调用方使用，保持 Manager 纯容器）
  // -----------------------------------------------------------------------

  /// @brief 施加 Neumann 源项（仅作用于非约束型 BC）
  static void applySources(BCManager &mgr,
                           double loadFactor, int currentStep = 0);

  /// @brief 施加 Dirichlet 约束（仅作用于约束型 BC）
  static void applyConstraints(BCManager &mgr,
                               double loadFactor, int currentStep = 0);

  /// @brief 通知当前活动步的 BC 提交载荷步结束状态
  static void commitEndStep(BCManager &mgr, int currentStep = 0);

protected:
  std::string name_; // 边界条件实例名称
};

/// @brief 边界条件存储条目（附带载荷步 ID）
struct BCEntry {
    int step;                  ///< 隶属的载荷步 (0 = 全局，始终生效)
    std::unique_ptr<BC> bc;    ///< 边界条件实例
};

} // namespace PDCommon::BC

#endif // PDCOMMON_BC_BC_H
