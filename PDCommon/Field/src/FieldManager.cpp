// ============================================================================
// FieldManager.cpp - 物理场管理器实现
// ============================================================================

#include "FieldManager.h"
#include "FieldRegistry.h"
#include "Logger.h"

namespace GRPD::Field {

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
// 工厂创建接口：通过 FieldRegistry 动态创建物理场
// ---------------------------------------------------------------------------
Field *FieldManager::createField(const std::string &typeName,
                                  const std::string &fieldName) {
    // 幂等：同名场已存在则直接返回
    auto it = fields_.find(fieldName);
    if (it != fields_.end()) {
        LOG_WARNING("[FieldManager] Field '" + fieldName +
                    "' already exists. Returning existing instance.");
        return it->second.get();
    }

    // 通过 FieldRegistry 工厂创建
    auto field = FieldRegistry::getInstance().createField(typeName, fieldName);
    if (!field) {
        LOG_ERROR("[FieldManager] Failed to create field '" + fieldName +
                  "' with type '" + typeName + "'.");
        return nullptr;
    }

    Field *ptr = field.get();
    fields_[fieldName] = std::move(field);
    LOG_INFO("[FieldManager] Created field '" + fieldName + "' (type: " +
             typeName + ").");
    return ptr;
}

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

} // namespace GRPD::Field
