// ============================================================================
// ThermalMat.cpp - 热传导微导率材料实现
// ============================================================================

#include "ThermalMat.h"
#include "FieldManager.h"
#include "Logger.h"
#include "MaterialRegistry.h"

namespace PDCommon::Material {

// ---------------------------------------------------------------------------
// 构造函数
// ---------------------------------------------------------------------------
ThermalMat::ThermalMat(const std::string &name) : Material(name) {
  // 物理参数已在头文件中初始化为 -1.0，具体数值将由外部(如 YAML)读取后赋值
}

// ---------------------------------------------------------------------------
// 初始化材料参数
// ---------------------------------------------------------------------------
void ThermalMat::initialize(const YAML::Node &matNode) {
  if (matNode["Conductivity"]) {
    conductivity_ = matNode["Conductivity"].as<double>();
  } else {
    LOG_WARNING("[ThermalMat] 'Conductivity' not found, using default.");
  }

  if (matNode["HeatCapacity"]) {
    heatCapacity_ = matNode["HeatCapacity"].as<double>();
  } else {
    LOG_WARNING("[ThermalMat] 'HeatCapacity' not found, using default.");
  }

  if (matNode["Density"]) {
    density_ = matNode["Density"].as<double>();
  } else {
    LOG_WARNING("[ThermalMat] 'Density' not found, using default.");
  }

  LOG_INFO("[ThermalMat] Material '" + name_ +
           "' is initialized. Conductivity = " + std::to_string(conductivity_) +
           ", HeatCapacity = " + std::to_string(heatCapacity_) +
           ", Density = " + std::to_string(density_));
}

// ---------------------------------------------------------------------------
// 分配材料状态变量场
// ---------------------------------------------------------------------------
void ThermalMat::allocateStateVariables(PDCommon::Field::FieldManager &fm) {
  // 注意：HeatFlux 已在 ThermalFields 中作为核心场注册，此处无需重复
}

size_t ThermalMat::getNumStateVariables() const {
  // 目前默认为 0，如需烧蚀等状态变量可在此扩充
  return 0;
}

// ---------------------------------------------------------------------------
// 本构关系/状态计算
// ---------------------------------------------------------------------------
void ThermalMat::evaluate() {
  // 热传导本构由于物理上被简化为线性常数(即微导率系数)，其核心的微分方程求解
  // 往往是由 solver/力态积分函数(如 ThermalEngine 或相应的积分器)执行的。
  // 因此这里的 evaluate
  // 可能暂时为空，或者处理更复杂的非线性热材料逻辑（如与温度相关的热导率）。
}

// ============================================================================
// 重要：将此材料通过宏静态注册到全局单例工厂 MaterialRegistry
// ============================================================================
// 这里的 "ThermalMat" 字符串必须与 YAML 文件中的 `Type: ThermalMat` 对应
REGISTER_MATERIAL_TYPE(ThermalMat, [](const std::string &name) {
  return std::make_unique<ThermalMat>(name);
});

// ---------------------------------------------------------------------------
// Getter / Setter
// ---------------------------------------------------------------------------
double ThermalMat::getConductivity() const { return conductivity_; }
void ThermalMat::setConductivity(double Con) { conductivity_ = Con; }
double ThermalMat::getHeatCapacity() const { return heatCapacity_; }
void ThermalMat::setHeatCapacity(double Cap) { heatCapacity_ = Cap; }
double ThermalMat::getDensity() const { return density_; }
void ThermalMat::setDensity(double rho) { density_ = rho; }

} // namespace PDCommon::Material
