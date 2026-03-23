#ifndef PDCOMMON_MATERIAL_THERMAL_MATERIAL_H
#define PDCOMMON_MATERIAL_THERMAL_MATERIAL_H

// ============================================================================
// ThermalMaterial.h - 热传导本构纯虚基类
//
// 职责：
//   定义所有热传导材料模型必须实现的纯数学接口。
//   输入温度梯度 ∇T，输出热流密度 q。
//
// 核心规约：
//   - 本构模型是纯粹的数学黑盒，绝对不包含粒子、邻居表、键等
//     离散化概念。
//   - 使用 Eigen::Vector3d 进行 3D 向量运算。
// ============================================================================

#include "Material.h"
#include <Eigen/Dense>

namespace PDCommon::Material {

class ThermalMaterial : public Material {
public:
  using Material::Material; // 继承基类构造函数

  virtual ~ThermalMaterial() = default;

  // -----------------------------------------------------------------------
  // 核心纯虚接口：纯数学本构计算
  // -----------------------------------------------------------------------

  /// @brief 计算热流密度
  /// @param gradT 温度梯度向量 ∇T [K/m]
  /// @return 热流密度向量 q [W/m²]
  /// @note 数学定义：q = f(∇T)，具体形式由派生类实现
  virtual Eigen::Vector3d ComputeHeatFlux(
      const Eigen::Vector3d& gradT) const = 0;

  // -----------------------------------------------------------------------
  // 材料物理参数纯虚接口
  // -----------------------------------------------------------------------

  /// @brief 获取热导率 k [W/(m·K)]
  virtual double getConductivity() const = 0;

  /// @brief 获取比热容 cp [J/(kg·K)]
  virtual double getHeatCapacity() const = 0;

  /// @brief 获取密度 ρ [kg/m³]
  virtual double getDensity() const = 0;
};

} // namespace PDCommon::Material

#endif // PDCOMMON_MATERIAL_THERMAL_MATERIAL_H
