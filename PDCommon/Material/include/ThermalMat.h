#ifndef THERMAL_MAT_H
#define THERMAL_MAT_H

// ============================================================================
// ThermalMat.h - 近场动力学热传导微导率材料模型
// 责任：
// 1. 定义热传导过程中的物理常数（如：微导率、热导率等）。
// 2. 继承 Material 接口，实现 evaluate 函数（预留）。
// 架构规约：本构模型与计算核心绝对解耦。
// ============================================================================

#include "Material.h"
#include <string>
#include <yaml-cpp/yaml.h>

namespace GRPD::Material {

class ThermalMat : public Material {
public:
  /// @brief 构造函数
  /// @param name 材料实例的具体名称（例如 YAML 中定义的别名）
  explicit ThermalMat(const std::string &name = "");

  virtual ~ThermalMat() = default;

  // -----------------------------------------------------------------------
  // 覆盖基类虚函数
  // -----------------------------------------------------------------------

  /// @brief 初始化材料参数或状态变量（若有特别参数需初始化则在此进行）
  /// @param matNode 属于此材料实体的 YAML 子节点
  void initialize(const YAML::Node &matNode) override;

  /// @brief 分配热传导材料的状态变量场 (HeatFlux)
  void allocateStateVariables(GRPD::Field::FieldManager &fm) override;

  /// @brief 计算力态或更新本构关系
  /// 这里热传导只提供物理参数，计算逻辑将在专门的 solver(Engine计算核心)中处理
  void evaluate() override;

  // -----------------------------------------------------------------------
  // 物理常数 Getter / Setter
  // -----------------------------------------------------------------------

  /// @brief 获取微导率 (Micro-conductivity)
  double getConductivity() const;

  /// @brief 设置微导率
  void setConductivity(double Con);

  // 也可根据需要扩充诸如 热容(HeatCapacity)、密度(Density) 等参数
  double getHeatCapacity() const;
  void setHeatCapacity(double Cap);

  double getDensity() const;
  void setDensity(double rho);

private:
  double conductivity_{-1.0}; // 热传导常数
  double heatCapacity_{-1.0}; // 比热容常数
  double density_{-1.0};      // 密度
};

} // namespace GRPD::Material

#endif // THERMAL_MAT_H
