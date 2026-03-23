// ============================================================================
// FourierThermalMat.cpp - 傅里叶热传导本构实现
//
// 实现 FourierThermalMat 类：
//   - ComputeHeatFlux: q = -k * gradT (傅里叶定律)
//   - initialize: 从 YAML 读取 k, cp, ρ
//   - 工厂注册: "FourierThermalMat"
// ============================================================================

#include "FourierThermalMat.h"
#include "FieldManager.h"
#include "Logger.h"
#include "MaterialRegistry.h"

namespace PDCommon::Material {

// ---------------------------------------------------------------------------
// 构造函数
// ---------------------------------------------------------------------------
FourierThermalMat::FourierThermalMat(const std::string& name)
    : ThermalMaterial(name) {
  // 物理参数已在头文件中初始化为 -1.0，具体数值将由外部(如 YAML)读取后赋值
}

// ---------------------------------------------------------------------------
// 核心本构计算：傅里叶热传导定律
// q = -k * ∇T
// ---------------------------------------------------------------------------
Eigen::Vector3d FourierThermalMat::ComputeHeatFlux(
    const Eigen::Vector3d& gradT) const {
  // 傅里叶热传导定律：热流密度与温度梯度成正比，方向相反
  return -conductivity_ * gradT;
}

// ---------------------------------------------------------------------------
// 初始化材料参数
// ---------------------------------------------------------------------------
void FourierThermalMat::initialize(const YAML::Node& matNode) {
  if (matNode["Conductivity"]) {
    conductivity_ = matNode["Conductivity"].as<double>();
  } else {
    LOG_WARNING("[FourierThermalMat] 'Conductivity' not found, using default.");
  }

  if (matNode["HeatCapacity"]) {
    heatCapacity_ = matNode["HeatCapacity"].as<double>();
  } else {
    LOG_WARNING("[FourierThermalMat] 'HeatCapacity' not found, using default.");
  }

  if (matNode["Density"]) {
    density_ = matNode["Density"].as<double>();
  } else {
    LOG_WARNING("[FourierThermalMat] 'Density' not found, using default.");
  }

  LOG_INFO("[FourierThermalMat] Material '" + name_ +
           "' is initialized. Conductivity = " +
           std::to_string(conductivity_) +
           ", HeatCapacity = " + std::to_string(heatCapacity_) +
           ", Density = " + std::to_string(density_));
}

// ---------------------------------------------------------------------------
// 分配材料状态变量场
// ---------------------------------------------------------------------------
void FourierThermalMat::allocateStateVariables(
    PDCommon::Field::FieldManager& fm) {
  // 注意：HeatFlux 已在 ThermalFields 中作为核心场注册，此处无需重复
}

size_t FourierThermalMat::getNumStateVariables() const {
  // 目前默认为 0，如需烧蚀等状态变量可在此扩充
  return 0;
}

// ---------------------------------------------------------------------------
// 本构关系/状态计算（预留）
// ---------------------------------------------------------------------------
void FourierThermalMat::evaluate() {
  // 热传导本构的核心计算通过 ComputeHeatFlux 接口执行
  // 此 evaluate 预留接口，可用于更复杂的非线性热材料逻辑
  // （如与温度相关的热导率 k(T) 的更新等）
}

// ============================================================================
// 重要：将此材料通过宏静态注册到全局单例工厂 MaterialRegistry
// ============================================================================
// 注册名 "FourierThermalMat" 必须与 YAML 文件中的 `Type: FourierThermalMat` 对应
REGISTER_MATERIAL_TYPE(FourierThermalMat, [](const std::string& name) {
  return std::make_unique<FourierThermalMat>(name);
});

// ---------------------------------------------------------------------------
// Getter / Setter
// ---------------------------------------------------------------------------
double FourierThermalMat::getConductivity() const { return conductivity_; }
void FourierThermalMat::setConductivity(double k) { conductivity_ = k; }

double FourierThermalMat::getHeatCapacity() const { return heatCapacity_; }
void FourierThermalMat::setHeatCapacity(double cp) { heatCapacity_ = cp; }

double FourierThermalMat::getDensity() const { return density_; }
void FourierThermalMat::setDensity(double rho) { density_ = rho; }

} // namespace PDCommon::Material
