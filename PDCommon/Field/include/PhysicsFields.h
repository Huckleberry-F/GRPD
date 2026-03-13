#ifndef GRPD_FIELD_PHYSICS_FIELDS_H
#define GRPD_FIELD_PHYSICS_FIELDS_H

// ============================================================================
// PhysicsFields.h - 物理场配置器基类
// 责任：
// 1. 定义特定物理问题（热传导、力学等）所需核心场的注册接口
// 2. 由 PhysicsFieldRegistry 管理，在初始化阶段由引擎按需调用
// 架构对标：对标 Material/BC 的基类 + 注册表设计模式
// ============================================================================

#include <string>

// 前置声明
namespace GRPD::Field {
class FieldManager;
}

namespace GRPD::Field {

class PhysicsFields {
public:
    explicit PhysicsFields(const std::string &typeName = "");

    virtual ~PhysicsFields();

    // 禁用拷贝
    PhysicsFields(const PhysicsFields &) = delete;
    PhysicsFields &operator=(const PhysicsFields &) = delete;

    /// @brief 向 FieldManager 注册此物理问题所需的所有核心场
    /// @param fm 物理场管理器引用
    virtual void registerFields(FieldManager &fm) = 0;

    /// @brief 获取类型名称
    const std::string &getTypeName() const { return typeName_; }

protected:
    std::string typeName_;
};

} // namespace GRPD::Field

#endif // GRPD_FIELD_PHYSICS_FIELDS_H
