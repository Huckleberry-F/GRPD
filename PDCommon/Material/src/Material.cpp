// ============================================================================
// Material.cpp - 材料基础类实现
// ============================================================================

#include "Material.h"
#include "DamageModel.h"
#include "DamageRegistry.h"
#include "Logger.h"

namespace PDCommon::Material {

// 基础类实现
Material::Material(const std::string &name) : name_(name), matId_(-1), damageModel_(nullptr) {}

Material::~Material() = default;

const std::string &Material::getName() const { return name_; }

void Material::setName(const std::string &name) { name_ = name; }

int Material::getMatId() const { return matId_; }

void Material::setMatId(int id) { matId_ = id; }

void Material::initialize(const YAML::Node &matNode) {
  // 解析依附于该材料独有的损伤准则
  if (matNode["Damage"]) {
      if (matNode["Damage"]["Type"]) {
          std::string damageTypeStr = matNode["Damage"]["Type"].as<std::string>();
          if (damageTypeStr != "None") {
               if (PDCommon::Damage::DamageRegistry::getInstance().contains(damageTypeStr)) {
                   damageModel_ = PDCommon::Damage::DamageRegistry::getInstance().create(damageTypeStr);
                   if (damageModel_) {
                       damageModel_->configure(matNode["Damage"]);
                       LOG_INFO("[Material] Attached DamageModel '" + damageTypeStr + "' to material: " + name_);
                   }
               } else {
                   LOG_ERROR("[Material] Damage model '" + damageTypeStr + "' not found in registry!");
               }
          }
      }
  }
}
PDCommon::Damage::DamageModel* Material::getDamageModel() const {
  return damageModel_.get();
}

void Material::allocateStateVariables(PDCommon::Field::FieldManager &fm) {
  // 默认空实现，子类覆盖以注册材料私有的特定场（非 SDV 池模式）
}

size_t Material::getNumStateVariables() const {
  // 默认返回 0，不需要池化状态变量
  return 0;
}

} // namespace PDCommon::Material
