#ifndef PDCOMMON_IO_INP_MESH_READER_H
#define PDCOMMON_IO_INP_MESH_READER_H

// ============================================================================
// InpMeshReader.h - Abaqus .inp 格式读取器（骨架）
//
// 架构对称于 GrpdMeshReader，预留给未来 Abaqus 导入支持。
// ============================================================================

#include "MeshReader.h"
#include <string>

namespace PDCommon::IO {

class InpMeshReader : public MeshReader {
public:
  /// @brief 构造函数
  explicit InpMeshReader(const std::string &name = "");

  ~InpMeshReader() override = default;

  /// @brief 读取 .inp 文件，填充 meshData_（当前为骨架实现）
  bool read(const std::string &filepath) override;
};

} // namespace PDCommon::IO

#endif // PDCOMMON_IO_INP_MESH_READER_H
