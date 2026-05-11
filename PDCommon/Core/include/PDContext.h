#ifndef PDCOMMON_CORE_PDCONTEXT_H
#define PDCOMMON_CORE_PDCONTEXT_H

// ============================================================================
// PDContext.h - 近场动力学仿真上下文容器 (核心枢纽)
// 责任：
// 1. 代表一个完整的 PD 物理或计算工程上下文
// 2. 独立持有自己专属的 ParticleManager、MaterialManager、FieldManager
// 等数据组件
// 3. 作为 PD 系统与外界 (如主程序、或者未来的 FEM 系统) 耦合的唯一数据桥梁
// ============================================================================

#include "BCManager.h"
#include "ContactManager.h"
#include "FieldManager.h"
#include "MaterialManager.h"
#include "NeighborList.h"
#include "ParticleManager.h"
#include <memory>
#include <string>

namespace PDCommon::Core {

class PDContext {
public:
  /// @brief 默认构造函数
  PDContext() = default;

  /// @brief 构造函数，初始化一个全新的仿真模型上下文
  /// @param name 模型名称，用于标识和日志输出
  explicit PDContext(const std::string &name);

  ~PDContext() = default;

  // 禁用拷贝，允许移动（保持资源独占）
  PDContext(const PDContext &) = delete;
  PDContext &operator=(const PDContext &) = delete;
  PDContext(PDContext &&) = default;
  PDContext &operator=(PDContext &&) = default;

  /// @brief 获取模型名称
  const std::string &getName() const { return name_; }

  /// @brief 设置模型名称
  void setName(const std::string &name) { name_ = name; }

  /// @brief 获取粒子管理器引用（非 const，用于求解和初始化）
  PDCommon::Model::ParticleManager &getParticleManager() {
    return particleManager_;
  }
  const PDCommon::Model::ParticleManager &getParticleManager() const {
    return particleManager_;
  }

  /// @brief 获取材料管理器引用（非 const，用于绑定和求解）
  PDCommon::Material::MaterialManager &getMaterialManager() {
    return materialManager_;
  }
  const PDCommon::Material::MaterialManager &getMaterialManager() const {
    return materialManager_;
  }

  // -----------------------------------------------------------------------
  // 物理场管理器 (FieldManager) — 统一管理所有物理场
  // -----------------------------------------------------------------------

  /// @brief 获取物理场管理器引用
  PDCommon::Field::FieldManager &getFieldManager() { return fieldManager_; }
  const PDCommon::Field::FieldManager &getFieldManager() const {
    return fieldManager_;
  }

  // -----------------------------------------------------------------------
  // 近邻列表 (NeighborList) 按需分配
  // -----------------------------------------------------------------------
  void createNeighborList(const PDCommon::Model::ParticleManager &mgr,
                          double horizon);
  bool hasNeighborList() const { return neighborList_ != nullptr; }
  PDCommon::Neighbor::NeighborList &getNeighborList() { return *neighborList_; }
  const PDCommon::Neighbor::NeighborList &getNeighborList() const {
    return *neighborList_;
  }

  // -----------------------------------------------------------------------
  // 边界条件管理器 (BCManager)
  // -----------------------------------------------------------------------
  PDCommon::BC::BCManager &getBCManager() { return bcManager_; }
  const PDCommon::BC::BCManager &getBCManager() const { return bcManager_; }

  // -----------------------------------------------------------------------
  // 接触系统管理器 (ContactManager)
  // -----------------------------------------------------------------------
  PDCommon::Contact::ContactManager &getContactManager() {
    return contactManager_;
  }
  const PDCommon::Contact::ContactManager &getContactManager() const {
    return contactManager_;
  }

  // -----------------------------------------------------------------------
  // 模型维度 (2D / 3D)
  // -----------------------------------------------------------------------

  /// @brief 获取模型维度 (2 或 3)
  int getDimension() const { return dimension_; }

  /// @brief 设置模型维度 (2 或 3)
  void setDimension(int dim) { dimension_ = dim; }

  /// @brief 获取 2D 模型厚度（3D 下无意义，默认 1.0）
  double getThickness() const { return thickness_; }

  /// @brief 设置 2D 模型厚度
  void setThickness(double h) { thickness_ = h; }

  // -----------------------------------------------------------------------
  // 当前时间步长（由 TimeIntegrator 每步更新，供接触等模块读取）
  // -----------------------------------------------------------------------

  /// @brief 获取当前时间步长
  double getCurrentDt() const { return currentDt_; }

  /// @brief 设置当前时间步长（由积分器在每步开头调用）
  void setCurrentDt(double dt) { currentDt_ = dt; }

  // -----------------------------------------------------------------------
  // 增量步冻结标志 (用于接触法向与权重在准静态子步内的几何锚定)
  // -----------------------------------------------------------------------
  bool isIncrementStart() const { return isIncrementStart_; }
  void setIncrementStart(bool v) { isIncrementStart_ = v; }

  // -----------------------------------------------------------------------
  // 本构状态冻结标志 (ADR 初始刚度法：内循环中冻结非线性本构)
  // -----------------------------------------------------------------------

  /// @brief 查询当前是否处于状态冻结模式
  /// @details 冻结时，力学本构应退化为纯弹性计算，跳过路径依赖的状态演化
  bool isStateFrozen() const { return stateFrozen_; }

  /// @brief 设置状态冻结标志
  /// @param frozen true=冻结（纯弹性），false=解冻（完整非线性本构）
  void setStateFrozen(bool frozen) { stateFrozen_ = frozen; }

  /// @brief 获取 ADR 外循环当前迭代次数
  int getOuterIter() const { return outerIter_; }

  /// @brief 设置 ADR 外循环当前迭代次数
  void setOuterIter(int iter) { outerIter_ = iter; }

private:
  std::string name_;        ///< 模型名称
  int dimension_ = 3;       ///< 模型维度 (默认 3D)
  double thickness_ = 1.0;  ///< 2D 模型厚度 (默认 1.0)
  double currentDt_ = 1e-9; ///< 当前时间步长（由积分器每步更新）
  PDCommon::Model::ParticleManager particleManager_;    ///< 粒子管理器
  PDCommon::Material::MaterialManager materialManager_; ///< 材料管理器
  PDCommon::Field::FieldManager fieldManager_;          ///< 物理场管理器

  std::unique_ptr<PDCommon::Neighbor::NeighborList>
      neighborList_; ///< 近邻列表（按需分配）

  PDCommon::BC::BCManager bcManager_;                ///< 边界条件管理器
  PDCommon::Contact::ContactManager contactManager_; ///< 接触系统管理器
  bool isIncrementStart_ =
      false; ///< 是否处于当前增量步（或物理物理时间步）的首个迭代
  bool stateFrozen_ = false; ///< 本构状态冻结标志（ADR 初始刚度法）
  int outerIter_ = 0;        ///< ADR 初始刚度法外循环迭代次数
};

} // namespace PDCommon::Core

#endif // PDCOMMON_CORE_PDCONTEXT_H
