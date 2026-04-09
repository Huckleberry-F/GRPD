// ============================================================================
// Material.cpp - 材料基础类实现
// ============================================================================

#include "Material.h"
#include "FractureModel.h"
#include "FractureRegistry.h"
#include "Logger.h"

namespace PDCommon::Material {

// 基础类实现
Material::Material(const std::string &name)
    : name_(name), matId_(-1), fractureModel_(nullptr) {}

Material::~Material() = default;

const std::string &Material::getName() const { return name_; }

void Material::setName(const std::string &name) { name_ = name; }

int Material::getMatId() const { return matId_; }

void Material::setMatId(int id) { matId_ = id; }

void Material::initialize(const YAML::Node &matNode) {
  // 解析依附于该材料独有的断裂准则 (新架构下键完整性评估属于 Fracture 模块)
  if (matNode["Fracture"]) {
    if (matNode["Fracture"]["Type"]) {
      std::string damageTypeStr =
          matNode["Fracture"]["Type"].as<std::string>();
      if (damageTypeStr != "None") {
        if (PDCommon::Fracture::FractureRegistry::getInstance().contains(
                damageTypeStr)) {
          fractureModel_ = PDCommon::Fracture::FractureRegistry::getInstance().create(
              damageTypeStr);
          if (fractureModel_) {
            fractureModel_->configure(matNode["Fracture"]);
            LOG_INFO("[Material] Attached FractureModel '" + damageTypeStr +
                     "' to material: " + name_);
          }
        } else {
          LOG_ERROR("[Material] Fracture model '" + damageTypeStr +
                    "' not found in registry!");
        }
      }
    }
  }
}
PDCommon::Fracture::FractureModel *Material::getFractureModel() const {
  return fractureModel_.get();
}

void Material::allocateStateVariables(PDCommon::Field::FieldManager &fm) {
  // 默认空实现，子类覆盖以注册材料私有的特定场（非 SDV 池模式）
}

size_t Material::getNumStateVariables() const {
  // 默认返回 0，不需要池化状态变量
  return 0;
}

} // namespace PDCommon::Material
