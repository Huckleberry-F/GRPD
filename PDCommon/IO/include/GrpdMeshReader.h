#ifndef PDCOMMON_IO_GRPD_MESH_READER_H
#define PDCOMMON_IO_GRPD_MESH_READER_H

// ============================================================================
// GrpdMeshReader.h - .grpd 格式读取器
//
// 架构对称于 FourierThermalMat（具体派生类）：
//   FourierThermalMat: 继承 ThermalMaterial，实现 ComputeHeatFlux()
//   GrpdMeshReader:    继承 MeshReader，实现 read()
//
// 适配器模式：内部委托已有的 GrpdReader 静态方法完成实际解析，
// 然后将 ParticleManager 数据搬运到通用 MeshData 中。
// ============================================================================

#include "MeshReader.h"
#include <string>

namespace PDCommon::IO {

class GrpdMeshReader : public MeshReader {
public:
  /// @brief 构造函数（对称 FourierThermalMat(name)）
  explicit GrpdMeshReader(const std::string &name = "");

  ~GrpdMeshReader() override = default;

  /// @brief 读取 .grpd 文件，填充 meshData_
  bool read(const std::string &filepath) override;
};

} // namespace PDCommon::IO

#endif // PDCOMMON_IO_GRPD_MESH_READER_H
