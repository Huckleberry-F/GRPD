#include "InpMeshReader.h"
#include "Logger.h"
#include "ReaderRegistry.h"

namespace PDCommon::IO {

// ---------------------------------------------------------------------------
// 构造函数
// ---------------------------------------------------------------------------
InpMeshReader::InpMeshReader(const std::string &name) : MeshReader(name) {}

// ---------------------------------------------------------------------------
// 读取 .inp 文件（骨架实现）
// ---------------------------------------------------------------------------
bool InpMeshReader::read(const std::string &filepath) {
  LOG_WARNING("[InpMeshReader] INP reader not yet implemented. File: " +
              filepath);
  return false;
}

// 编译期自动注册：以 ".inp" 后缀名为键
REGISTER_READER(".inp", InpMeshReader)

} // namespace PDCommon::IO
