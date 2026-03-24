// ============================================================================
// FieldManager.cpp - 物理场管理器实现（纯容器，不负责创建）
// ============================================================================

#include "FieldManager.h"
#include "Logger.h"

namespace PDCommon::Field {

// ---------------------------------------------------------------------------
// 实例添加接口（对标 MaterialManager::addMaterial）
// ---------------------------------------------------------------------------
Field *FieldManager::addField(std::unique_ptr<Field> field) {
    if (!field) {
        LOG_ERROR("[FieldManager] Cannot add null field.");
        return nullptr;
    }

    const std::string &name = field->getName();

    // 幂等：同名场已存在则直接返回已有指针
    auto it = fields_.find(name);
    if (it != fields_.end()) {
        LOG_WARNING("[FieldManager] Field '" + name +
                    "' already exists. Returning existing instance.");
        return it->second.get();
    }

    Field *ptr = field.get();
    fields_[name] = std::move(field);
    LOG_INFO("[FieldManager] Added field '" + name + "'.");
    return ptr;
}

// ---------------------------------------------------------------------------
// 查询接口
// ---------------------------------------------------------------------------
Field *FieldManager::getField(const std::string &name) {
    auto it = fields_.find(name);
    return (it != fields_.end()) ? it->second.get() : nullptr;
}

const Field *FieldManager::getField(const std::string &name) const {
    auto it = fields_.find(name);
    return (it != fields_.end()) ? it->second.get() : nullptr;
}

bool FieldManager::hasField(const std::string &name) const {
    return fields_.find(name) != fields_.end();
}

std::vector<std::string> FieldManager::getFieldNames() const {
    std::vector<std::string> names;
    names.reserve(fields_.size());
    for (const auto &pair : fields_) {
        names.push_back(pair.first);
    }
    return names;
}

size_t FieldManager::getFieldCount() const { return fields_.size(); }

// ---------------------------------------------------------------------------
// 批量操作
// ---------------------------------------------------------------------------
void FieldManager::resizeAll(size_t numParticles) {
    for (auto &pair : fields_) {
        pair.second->resize(numParticles);
    }
    LOG_INFO("[FieldManager] Resized all " + std::to_string(fields_.size()) +
             " fields for " + std::to_string(numParticles) + " particles.");
}

} // namespace PDCommon::Field
