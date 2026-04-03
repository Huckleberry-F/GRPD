#ifndef PDCOMMON_MATERIAL_H
#define PDCOMMON_MATERIAL_H

// ============================================================================
// Material.h - 材料基础接口
// 责任：定义所有近场动力学(PD)本构模型（如弹塑性、脆性、蠕变等）的基本行为。
// 架构规约：本构模型与计算核心绝对解耦，主计算循环只能通过此基类的虚函数调用物理逻辑。
// ============================================================================

#include <string>
#include <yaml-cpp/yaml.h>

// 前置声明，避免在基类头文件引入具体物理场依赖
namespace PDCommon::Field {
class FieldManager;
}

namespace PDCommon::Damage {
class DamageModel;
}

namespace PDCommon::Material {

class Material {
public:
  /// @brief 构造函数
  /// @param name 材料实例的唯一名称
  explicit Material(const std::string &name = "");

  virtual ~Material();

  // 禁用拷贝构造和赋值操作
  Material(const Material &) = delete;
  Material &operator=(const Material &) = delete;

  /// @brief 获取当前材料实例名称
  const std::string &getName() const;

  /// @brief 设置名字
  void setName(const std::string &name);

  /// @brief 获取/设置 MatID
  int getMatId() const;
  void setMatId(int id);

  /// @brief 初始化材料参数或状态变量
  /// @param matNode 对应于本材料的 YAML 配置节点
  virtual void initialize(const YAML::Node &matNode);

  /// @brief 分配材料私有的状态变量场
  /// 子类覆盖此方法，向 FieldManager 注册材料所需的状态变量
  /// 注意：如果使用通用 SDV 池化机制，此方法可能为空，主要用于注册特殊命名的场
  /// @param fm 物理场管理器引用
  virtual void allocateStateVariables(PDCommon::Field::FieldManager &fm);

  /// @brief 将场数据指针对接到材料内部的常驻裸指针池中（用于规避极高频求解过程中的虚函数查询与表查找）
  /// 在所有材料向 fm 注册完毕且 fm 执行 Resize 后调用
  virtual void bindStateVariables(PDCommon::Field::FieldManager &fm) {}

  /// @brief 获取该材料需要的状态变量(SDV)数量
  /// 引擎将根据所有材料的最大需求分配统一的 SDV 场
  /// @return 状态变量数量
  virtual size_t getNumStateVariables() const;

  /// @brief 计算力态或更新本构关系 (子类需具体实现)
  /// 注意：后续应结合Particle对象的属性（如状态变量state_vars等）传入进行计算
  virtual void evaluate() = 0;

  /// @brief 提交状态变量（ADR 载荷步收敛后调用）
  /// @details 将试探性状态变量（State_Trial）刻录为已收敛的真实状态（State_Old）。
  ///          默认空实现。路径依赖材料（如 J2 塑性、损伤演化）必须重写此方法。
  virtual void commitState() {}

  /// @brief 获取挂载在此材料下的损伤评估模型
  PDCommon::Damage::DamageModel* getDamageModel() const;

protected:
  std::string name_; // 材料实例名称
  int matId_;        // 关联的整型标号
  std::unique_ptr<PDCommon::Damage::DamageModel> damageModel_; // 专属损伤判定模型
};

} // namespace PDCommon::Material

#endif // PDCOMMON_MODEL_PDCOMMON_MATERIAL_H
