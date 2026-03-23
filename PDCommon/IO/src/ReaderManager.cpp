#include "ReaderManager.h"
#include "Logger.h"

namespace PDCommon::IO {

// ---------------------------------------------------------------------------
// 添加读取器实例
// ---------------------------------------------------------------------------
void ReaderManager::addReader(const std::string &name,
                              std::unique_ptr<MeshReader> reader) {
  if (readers_.count(name)) {
    LOG_WARNING("[ReaderManager] Overwriting existing reader: " + name);
  }
  reader->setName(name);
  readers_[name] = std::move(reader);
  LOG_INFO("[ReaderManager] Added reader: " + name);
}

// ---------------------------------------------------------------------------
// 获取读取器实例
// ---------------------------------------------------------------------------
MeshReader *ReaderManager::getReader(const std::string &name) {
  auto it = readers_.find(name);
  if (it != readers_.end()) {
    return it->second.get();
  }
  LOG_ERROR("[ReaderManager] Reader not found: " + name);
  return nullptr;
}

const MeshReader *ReaderManager::getReader(const std::string &name) const {
  auto it = readers_.find(name);
  if (it != readers_.end()) {
    return it->second.get();
  }
  LOG_ERROR("[ReaderManager] Reader not found: " + name);
  return nullptr;
}

// ---------------------------------------------------------------------------
// 检查读取器是否存在
// ---------------------------------------------------------------------------
bool ReaderManager::hasReader(const std::string &name) const {
  return readers_.count(name) > 0;
}

} // namespace PDCommon::IO
