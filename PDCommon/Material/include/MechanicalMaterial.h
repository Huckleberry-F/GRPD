#ifndef PDCOMMON_MATERIAL_MECHANICAL_MATERIAL_H
#define PDCOMMON_MATERIAL_MECHANICAL_MATERIAL_H

// ============================================================================
// MechanicalMaterial.h - 力学本构纯虚基类
//
// 职责：
//   定义所有力学本构模型必须实现的纯数学接口。
//   支持小变形应变-应力转换和基于变形梯度的 P-K 应力转换。
//
// 核心规约：
//   - 本构模型是纯粹的数学黑盒，仅处理状态张量转换
//   - 适配态基近场动力学积分框架
// ============================================================================

#include "Material.h"
#include <Eigen/Dense>

namespace PDCommon::Material {

class MechanicalMaterial : public Material {
public:
  using Material::Material; // 继承基类构造函数

  virtual ~MechanicalMaterial() = default;

  // -----------------------------------------------------------------------
  // 核心纯虚接口：纯数学本构计算
  // -----------------------------------------------------------------------

  /// @brief 计算柯西应力/真实应力 (Cauchy Stress)
  /// @param strain 应变张量
  /// @return 应力张量
  virtual Eigen::Matrix3d
  ComputeEngineeringStress(const Eigen::Matrix3d &strain) const = 0;

  /// @brief 计算第一类 Piola-Kirchhoff 应力 P (NOSB中用于力态积分)
  /// @param F 变形梯度张量 (Deformation Gradient)
  /// @param particleId 发生变形物态的粒子全域编号，供路径依赖材料调用历史内变量（如果需要）
  /// @param stateMode 本构计算模式 (0: 正常, 1: 冻结并读取Old, 2: 冻结并读取Trial)
  /// @return 第一类 P-K 应力张量 P
  virtual Eigen::Matrix3d ComputePK1Stress(const Eigen::Matrix3d &F, int particleId = -1, int stateMode = 0) const = 0;

  // -----------------------------------------------------------------------
  // 材料物理参数纯虚接口
  // -----------------------------------------------------------------------

  /// @brief 获取密度 ρ [kg/m³]
  virtual double getDensity() const = 0;

  /// @brief 获取杨氏模量 E [GPa]
  virtual double getYoungsModulus() const = 0;

  /// @brief 获取泊松比 ν [-]
  virtual double getPoissonsRatio() const = 0;

  /// @brief 便捷获取体积模量 K [GPa]
  virtual double getBulkModulus() const {
    double E = getYoungsModulus();
    double nu = getPoissonsRatio();
    if (nu >= 0.5)
      return E * 1000.0; // 避免不可压缩失效
    return E / (3.0 * (1.0 - 2.0 * nu));
  }
};

} // namespace PDCommon::Material

#endif // PDCOMMON_MATERIAL_MECHANICAL_MATERIAL_H
