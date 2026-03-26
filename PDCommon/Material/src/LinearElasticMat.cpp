// ============================================================================
// LinearElasticMat.cpp - 各向同性线弹性本构实现
// ============================================================================

#include "LinearElasticMat.h"
#include "FieldManager.h"
#include "Logger.h"
#include "MaterialRegistry.h"

namespace PDCommon::Material {

LinearElasticMat::LinearElasticMat(const std::string &name)
    : MechanicalMaterial(name) {}

void LinearElasticMat::initialize(const YAML::Node &matNode) {
  if (matNode["Density"]) {
    density_ = matNode["Density"].as<double>();
  } else {
    LOG_WARNING("[LinearElasticMat] 'Density' not found, using default.");
  }

  if (matNode["YoungsModulus"]) {
    youngsModulus_ = matNode["YoungsModulus"].as<double>();
  } else {
    LOG_WARNING("[LinearElasticMat] 'YoungsModulus' not found, using default.");
  }

  if (matNode["PoissonsRatio"]) {
    poissonsRatio_ = matNode["PoissonsRatio"].as<double>();
  } else {
    LOG_WARNING("[LinearElasticMat] 'PoissonsRatio' not found, using default.");
  }

  computeLameParameters();

  LOG_INFO("[LinearElasticMat] Material '" + name_ +
           "' is initialized. E = " + std::to_string(youngsModulus_) +
           ", nu = " + std::to_string(poissonsRatio_) +
           ", Density = " + std::to_string(density_));
}

void LinearElasticMat::computeLameParameters() {
  if (youngsModulus_ > 0.0 && poissonsRatio_ > 0.0) {
    mu_ = youngsModulus_ / (2.0 * (1.0 + poissonsRatio_));
    lambda_ = (youngsModulus_ * poissonsRatio_) /
              ((1.0 + poissonsRatio_) * (1.0 - 2.0 * poissonsRatio_));
  }
}

Eigen::Matrix3d
LinearElasticMat::ComputeEngineeringStress(const Eigen::Matrix3d &F) const {
  // 广义胡克定律: \sigma = \lambda * tr(\varepsilon) * I + 2 * \mu *
  // \varepsilon
  Eigen::Matrix3d strain =
      0.5 * (F + F.transpose()) - Eigen::Matrix3d::Identity();
  double trace = strain.trace();
  return lambda_ * trace * Eigen::Matrix3d::Identity() + 2.0 * mu_ * strain;
}

Eigen::Matrix3d
LinearElasticMat::ComputePK1Stress(const Eigen::Matrix3d &F) const {
  // 小应变假设下的简易回拉（大变形应使用对数应变等其他形变度量）
  // 计算右 Cauchy-Green 张量 C = F^T * F
  Eigen::Matrix3d C = F.transpose() * F;
  // 计算 Green-Lagrange 应变张量 E = 0.5 * (C - I)
  Eigen::Matrix3d E = 0.5 * (C - Eigen::Matrix3d::Identity());

  // 第二类 Piola-Kirchhoff 应力 S
  double trace = E.trace();
  Eigen::Matrix3d S =
      lambda_ * trace * Eigen::Matrix3d::Identity() + 2.0 * mu_ * E;
  // 第一类 Piola-Kirchhoff 应力 P = F * S
  return F * S;
}

void LinearElasticMat::allocateStateVariables(
    PDCommon::Field::FieldManager &fm) {
  // 线弹性暂无状态变量
}

size_t LinearElasticMat::getNumStateVariables() const { return 0; }

void LinearElasticMat::evaluate() {
  // 预留接口：本构具体运算通过 ComputePK1Stress 暴露给 Kernel
  // 计算，无需在此缓存状态
}

double LinearElasticMat::getDensity() const { return density_; }
double LinearElasticMat::getYoungsModulus() const { return youngsModulus_; }
double LinearElasticMat::getPoissonsRatio() const { return poissonsRatio_; }

void LinearElasticMat::setDensity(double rho) { density_ = rho; }

void LinearElasticMat::setYoungsModulus(double e) {
  youngsModulus_ = e;
  computeLameParameters();
}

void LinearElasticMat::setPoissonsRatio(double nu) {
  poissonsRatio_ = nu;
  computeLameParameters();
}

// ============================================================================
// 将线弹性材料静态注册到全局单例工厂
// ============================================================================
REGISTER_MATERIAL_TYPE(LinearElasticMat, [](const std::string &name) {
  return std::make_unique<LinearElasticMat>(name);
});

} // namespace PDCommon::Material
