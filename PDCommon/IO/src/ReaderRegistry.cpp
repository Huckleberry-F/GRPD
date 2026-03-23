#include "ReaderRegistry.h"
#include "Logger.h"

namespace PDCommon::IO {

// ---------------------------------------------------------------------------
// 单例
// ---------------------------------------------------------------------------
ReaderRegistry &ReaderRegistry::getInstance() {
  static ReaderRegistry instance;
  return instance;
}

// ---------------------------------------------------------------------------
// 注册读取器类型（对称 registerMaterialType）
// ---------------------------------------------------------------------------
void ReaderRegistry::registerReaderType(const std::string &extension,
                                        ReaderCreatorFunc creator) {
  registry_[extension] = std::move(creator);
  LOG_INFO("[ReaderRegistry] Registered reader for: " + extension);
}

// ---------------------------------------------------------------------------
// 创建读取器实例（对称 createMaterial）
// ---------------------------------------------------------------------------
std::unique_ptr<MeshReader>
ReaderRegistry::createReader(const std::string &extension,
                             const std::string &name) {
  auto it = registry_.find(extension);
  if (it != registry_.end()) {
    LOG_INFO("[ReaderRegistry] Creating reader for: " + extension +
             " (name: " + name + ")");
    return it->second(name);
  }
  LOG_ERROR("[ReaderRegistry] No reader registered for extension: " +
            extension);
  return nullptr;
}

// ---------------------------------------------------------------------------
// 辅助查询
// ---------------------------------------------------------------------------
bool ReaderRegistry::hasType(const std::string &extension) const {
  return registry_.count(extension) > 0;
}

std::vector<std::string> ReaderRegistry::getRegisteredTypes() const {
  std::vector<std::string> types;
  types.reserve(registry_.size());
  for (const auto &pair : registry_) {
    types.push_back(pair.first);
  }
  return types;
}

} // namespace PDCommon::IO
