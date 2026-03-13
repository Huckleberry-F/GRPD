// ============================================================================
// Material.cpp - 材料基础类实现
// ============================================================================

#include "Material.h"

namespace GRPD::Material {

// 基础类实现
Material::Material(const std::string &name) : name_(name), matId_(-1) {}

const std::string &Material::getName() const { return name_; }

void Material::setName(const std::string &name) { name_ = name; }

int Material::getMatId() const { return matId_; }

void Material::setMatId(int id) { matId_ = id; }

void Material::initialize(const YAML::Node &matNode) {
  // 默认空实现，子类可覆盖以处理具体的 YAML 数据
}

void Material::allocateStateVariables(GRPD::Field::FieldManager &fm) {
  // 默认空实现，子类覆盖以注册材料私有的状态变量场
  // 例如：ThermalMat 注册 HeatFlux, ElasticMat 注册 Stress/Strain
}

} // namespace GRPD::Material
