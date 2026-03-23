#include "GrpdMeshReader.h"
#include "GrpdReader.h"
#include "Logger.h"
#include "ParticleManager.h"
#include "ReaderRegistry.h"

namespace PDCommon::IO {

// ---------------------------------------------------------------------------
// 构造函数（对称 FourierThermalMat(name)）
// ---------------------------------------------------------------------------
GrpdMeshReader::GrpdMeshReader(const std::string &name) : MeshReader(name) {}

// ---------------------------------------------------------------------------
// 读取 .grpd 文件，委托 GrpdReader 后填充 meshData_
// ---------------------------------------------------------------------------
bool GrpdMeshReader::read(const std::string &filepath) {
  LOG_INFO("[GrpdMeshReader] Reading file: " + filepath);

  // 1. 委托已有的静态 GrpdReader 完成底层解析
  PDCommon::Model::ParticleManager pm;
  if (!GrpdReader::read(filepath, pm)) {
    LOG_ERROR("[GrpdMeshReader] GrpdReader::read() failed for: " + filepath);
    return false;
  }

  // 2. 将 ParticleManager 中的数据转移到通用 meshData_
  meshData_.clear();
  const size_t n = pm.getTotalParticles();
  const auto &particles = pm.getAllParticles();

  meshData_.coords.resize(n * 3);
  meshData_.nodeIDs.resize(n);
  meshData_.partIDs.resize(n);
  meshData_.matIDs.resize(n);
  meshData_.volumes.resize(n);

  for (size_t i = 0; i < n; ++i) {
    const auto &p = particles[i];
    const auto &c = p.getCoords();
    meshData_.coords[i * 3 + 0] = c[0];
    meshData_.coords[i * 3 + 1] = c[1];
    meshData_.coords[i * 3 + 2] = c[2];
    meshData_.nodeIDs[i] = p.getId();
    meshData_.partIDs[i] = p.getPartId();
    meshData_.matIDs[i] = p.getMatId();
    meshData_.volumes[i] = p.getVolume();
  }

  LOG_INFO("[GrpdMeshReader] Successfully loaded " + std::to_string(n) +
           " particles into MeshData.");
  return true;
}

// 编译期自动注册：以 ".grpd" 后缀名为键
REGISTER_READER(".grpd", GrpdMeshReader)

} // namespace PDCommon::IO
