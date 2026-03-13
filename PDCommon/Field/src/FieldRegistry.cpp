// ============================================================================
// FieldRegistry.cpp - 物理场全局注册中心实现 + 标准场类型静态注册
// ============================================================================

#include "FieldRegistry.h"
#include "TypedField.h"
#include "Logger.h"
#include <stdexcept>

namespace GRPD::Field {

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
  } else {
    LOG_INFO("[FieldRegistry] Field type '" + type + "' registered.");
  }
  registryMap_[type] = creator;
}

// ---------------------------------------------------------------------------
// 通过工厂创建物理场实例
// ---------------------------------------------------------------------------
std::unique_ptr<Field> FieldRegistry::createField(const std::string &type,
                                                    const std::string &name) {
  auto it = registryMap_.find(type);
  if (it == registryMap_.end()) {
    LOG_ERROR("[FieldRegistry] Create Field failed: unknown type '" + type +
              "'!");
    throw std::runtime_error("Unknown Field type: " + type);
  }
  return it->second(name);
}

// ============================================================================
// 标准场类型的编译期静态注册（对标 BC/Material 的 REGISTER 宏）
// ============================================================================

// 标量场 (dim=1)：用于温度、损伤、压力等单分量变量
REGISTER_FIELD_TYPE(ScalarField, [](const std::string &name) {
  return std::make_unique<TypedField<double>>(name, 1);
});

// 三维矢量场 (dim=3)：用于位移、速度、加速度、热流矢量等
REGISTER_FIELD_TYPE(VectorField, [](const std::string &name) {
  return std::make_unique<TypedField<double>>(name, 3);
});

// 对称张量场 (dim=6)：用于应力、应变（Voigt 记法：xx yy zz xy xz yz）
REGISTER_FIELD_TYPE(SymTensorField, [](const std::string &name) {
  return std::make_unique<TypedField<double>>(name, 6);
});

// 整数标量场 (dim=1)：用于损伤标记、材料状态标志等离散变量
REGISTER_FIELD_TYPE(IntField, [](const std::string &name) {
  return std::make_unique<TypedField<int>>(name, 1);
});

} // namespace GRPD::Field
