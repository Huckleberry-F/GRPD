// ============================================================================
// PhysicsFields.cpp - 物理场配置器基类实现
// ============================================================================

#include "PhysicsFields.h"

namespace GRPD::Field {

PhysicsFields::PhysicsFields(const std::string &typeName)
    : typeName_(typeName) {}

// 在 .cpp 中定义虚析构函数，确保虚函数表 (vtable) 有固定的锚点
PhysicsFields::~PhysicsFields() = default;

} // namespace GRPD::Field
