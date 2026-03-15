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
  // 默认空实现，子类覆盖以注册材料私有的特定场（非 SDV 池模式）
}

size_t Material::getNumStateVariables() const {
  // 默认返回 0，不需要池化状态变量
  return 0;
}

} // namespace GRPD::Material
