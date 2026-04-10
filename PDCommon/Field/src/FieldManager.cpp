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

  // 同名场已存在则直接返回已有指针
  auto it = fields_.find(name);
  if (it != fields_.end()) {
    LOG_INFO("[FieldManager] Memory shared: Field '" + name +
             "' linked to existing SoA instance.");
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

// ---------------------------------------------------------------------------
// 全局 Swap 注册与执行机制
// ---------------------------------------------------------------------------

void FieldManager::registerSwapPair(const std::string &oldField,
                                    const std::string &trialField) {
  swapPairs_.insert({oldField, trialField});
  LOG_INFO("[FieldManager] Registered swap pair: " + oldField + " <-> " +
           trialField);
}

void FieldManager::executeAllRegisteredSwaps() {
  if (swapPairs_.empty())
    return;

  for (const auto &pair : swapPairs_) {
    const std::string &oldName = pair.first;
    const std::string &trialName = pair.second;

    auto *oldFieldBase = getField(oldName);
    auto *trialFieldBase = getField(trialName);

    if (!oldFieldBase || !trialFieldBase) {
      LOG_WARNING("[FieldManager] Swap failed: missing field " + oldName +
                  " or " + trialName);
      continue;
    }

    // Since we only know it's a DoubleField (dim>0), we cast it
    // Note: all our fields are TypedField<double> so this is safe
    auto *oldField = dynamic_cast<TypedField<double> *>(oldFieldBase);
    auto *trialField = dynamic_cast<TypedField<double> *>(trialFieldBase);

    if (oldField && trialField) {
      oldField->swapDataWith(*trialField);
    } else {
      LOG_WARNING("[FieldManager] Swap failed: type mismatch for " + oldName);
    }
  }
}

} // namespace PDCommon::Field
