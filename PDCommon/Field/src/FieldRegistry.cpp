// ============================================================================
// FieldRegistry.cpp - 物理场全局注册中心实现 + 标准场类型静态注册
// ============================================================================

#include "FieldRegistry.h"
#include "Logger.h"
#include "TypedField.h"
#include <stdexcept>

namespace PDCommon::Field {

// ---------------------------------------------------------------------------
// 单例实例获取
// ---------------------------------------------------------------------------
FieldRegistry &FieldRegistry::getInstance() {
  static FieldRegistry instance;
  return instance;
}

// ---------------------------------------------------------------------------
// 注册物理场工厂函数
// ---------------------------------------------------------------------------
void FieldRegistry::registerFieldType(const std::string &type,
                                      FieldCreatorFunc creator) {
  if (registryMap_.find(type) != registryMap_.end()) {
    LOG_WARNING("[FieldRegistry] Field type '" + type +
                "' already registered, overwriting.");
  }
  registryMap_[type] = creator;
}

// ---------------------------------------------------------------------------
// 通过工厂创建物理场实例
// ---------------------------------------------------------------------------
std::unique_ptr<Field> FieldRegistry::createField(const std::string &type,
                                                  const std::string &name,
                                                  int dim) {
  auto it = registryMap_.find(type);
  if (it == registryMap_.end()) {
    LOG_ERROR("[FieldRegistry] Create Field failed: unknown type '" + type +
              "'!");
    throw std::runtime_error("Unknown Field type: " + type);
  }
  return it->second(name, dim);
}

std::vector<std::string> FieldRegistry::getRegisteredTypes() const {
  std::vector<std::string> types;
  types.reserve(registryMap_.size());
  for (const auto &[key, _] : registryMap_) {
    types.push_back(key);
  }
  return types;
}

// ============================================================================
// 标准场类型的编译期静态注册
// ============================================================================

// 双精度场：支持任意维度（标量/矢量/张量/SDV 等）
REGISTER_FIELD_TYPE(DoubleField, [](const std::string &name, int dim) {
  return std::make_unique<TypedField<double>>(name, dim);
});

// 整数场：支持任意维度（ID、PartID 等离散变量）
REGISTER_FIELD_TYPE(IntField, [](const std::string &name, int dim) {
  return std::make_unique<TypedField<int>>(name, dim);
});

} // namespace PDCommon::Field
