#include "MeshReader.h"
#include "Logger.h"

namespace PDCommon::IO {

// ---------------------------------------------------------------------------
// 构造函数
// ---------------------------------------------------------------------------
MeshReader::MeshReader(const std::string &name) : name_(name) {}

// ---------------------------------------------------------------------------
// 名称访问
// ---------------------------------------------------------------------------
const std::string &MeshReader::getName() const { return name_; }

void MeshReader::setName(const std::string &name) { name_ = name; }

// ---------------------------------------------------------------------------
// 默认初始化（子类可覆盖）
// ---------------------------------------------------------------------------
void MeshReader::initialize(const YAML::Node &config) {
  // 默认空实现，子类按需覆盖以解析额外配置项
  (void)config;
}

} // namespace PDCommon::IO
