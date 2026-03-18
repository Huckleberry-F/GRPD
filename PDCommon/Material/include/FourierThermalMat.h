#ifndef PDCOMMON_MATERIAL_FOURIER_THERMAL_MAT_H
#define PDCOMMON_MATERIAL_FOURIER_THERMAL_MAT_H

// ============================================================================
// FourierThermalMat.h - 傅里叶热传导派生类
//
// 职责：
//   实现经典傅里叶热传导定律：q = -k ∇T
//
// 架构规约：
//   - 继承 ThermalMaterial 纯虚接口
//   - 本构模型是纯粹的数学黑盒
// ============================================================================

#include "ThermalMaterial.h"
#include <string>
#include <yaml-cpp/yaml.h>

namespace PDCommon::Material {

class FourierThermalMat : public ThermalMaterial {
public:
  /// @brief 构造函数
  /// @param name 材料实例的具体名称（例如 YAML 中定义的别名）
  explicit FourierThermalMat(const std::string& name = "");

  ~FourierThermalMat() override = default;

  // -----------------------------------------------------------------------
  // 实现 ThermalMaterial 纯虚接口
  // -----------------------------------------------------------------------

  /// @brief 计算傅里叶热流密度：q = -k * gradT
  /// @param gradT 温度梯度向量 ∇T [K/m]
  /// @return 热流密度向量 q = -k ∇T [W/m²]
  Eigen::Vector3d ComputeHeatFlux(
      const Eigen::Vector3d& gradT) const override;

  // -----------------------------------------------------------------------
  // 实现 Material 基类虚函数
  // -----------------------------------------------------------------------

  /// @brief 从 YAML 节点初始化材料参数
  /// @param matNode 属于此材料实体的 YAML 子节点
  void initialize(const YAML::Node& matNode) override;

  /// @brief 分配状态变量场（热传导模型暂无私有状态变量）
  void allocateStateVariables(PDCommon::Field::FieldManager& fm) override;

  /// @brief 获取状态变量数量
  size_t getNumStateVariables() const override;

  /// @brief 计算本构关系（预留接口，热传导通过 ComputeHeatFlux 执行）
  void evaluate() override;

  // -----------------------------------------------------------------------
  // 实现 ThermalMaterial 参数接口
  // -----------------------------------------------------------------------
  double getConductivity() const override;
  double getHeatCapacity() const override;
  double getDensity() const override;

  // -----------------------------------------------------------------------
  // Setter（仅 FourierThermalMat 拥有，基类不暴露）
  // -----------------------------------------------------------------------
  void setConductivity(double k);
  void setHeatCapacity(double cp);
  void setDensity(double rho);

private:
  double conductivity_{-1.0}; ///< 热导率 k [W/(m·K)]
  double heatCapacity_{-1.0}; ///< 比热容 cp [J/(kg·K)]
  double density_{-1.0};      ///< 密度 ρ [kg/m³]
};

} // namespace PDCommon::Material

#endif // PDCOMMON_MATERIAL_FOURIER_THERMAL_MAT_H
