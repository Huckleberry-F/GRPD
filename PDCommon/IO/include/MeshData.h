#ifndef PDCOMMON_IO_MESH_DATA_H
#define PDCOMMON_IO_MESH_DATA_H

// ============================================================================
// MeshData.h - 通用网格中间数据结构
//
// 设计灵感：借鉴 Writer 基类的 pointsVariable 模式。
// 无论读取 .grpd (点云)、.inp (Abaqus)、.cdb (ANSYS)，
// 解析结果统一存储在此结构中，作为 Reader 与 Engine 之间的解耦桥梁。
//
// Reader 侧：子类格式化读入 → 填充 MeshData
// Engine 侧：通过 getMeshData() 统一取用，无需关心原始文件格式
// ============================================================================

#include <string>
#include <vector>

// 前置声明（避免头文件循环依赖）
namespace PDCommon::Field {
class FieldManager;
}
namespace PDCommon::Model {
class ParticleManager;
}

namespace PDCommon::IO {

/// @brief 单元类型枚举（未来 FEM 扩展使用）
enum class ElementType {
  POINT = 0, // 物质点（无连接性）
  TET4,      // 四面体
  TET10,     // 十节点四面体
  HEX8,      // 六面体
  HEX20,     // 二十节点六面体
  TRI3,      // 三角形
  QUAD4,     // 四边形
};

/// @brief 载荷/边界条件条目
struct LoadEntry {
  int nodeID;       ///< 作用节点 ID
  int step;         ///< 隶属的载荷步（0为全局共有）
  int bcID;         ///< 边界条件组 ID
  std::string type; ///< 边界条件类型 ("T", "FLUX" 等)
  std::vector<double> values; ///< 多维边界条件值数组
  std::vector<std::string> tableNames; ///< 绑定的 Table 名称（如为空则为 "None"）
};

/// @brief 通用网格中间数据结构
///
/// 所有 MeshReader 子类将解析结果填入此结构。
/// Engine 侧通过 reader->getMeshData() 统一消费。
struct MeshData {
  // =======================================================================
  // 所有格式共有（点云 + FEM 网格）
  // =======================================================================
  std::vector<double> coords;   ///< 节点坐标 (3N，连续存储 x0,y0,z0,x1,y1,z1...)
  std::vector<int> nodeIDs;     ///< 节点全局 ID
  std::vector<int> partIDs;     ///< Part 归属 ID
  std::vector<int> matIDs;      ///< 材料 ID
  std::vector<double> volumes;  ///< 物质点代表体积

  // =======================================================================
  // FEM 网格专属（.inp / .cdb，点云格式留空即可）
  // =======================================================================
  std::vector<int> elements;       ///< 单元连接性（展平存储，节点 ID 序列）
  std::vector<int> elemOffsets;    ///< 每个单元在 elements 中的起始偏移
  std::vector<ElementType> elemTypes; ///< 每个单元的类型

  // =======================================================================
  // 边界条件 / 载荷
  // =======================================================================
  std::vector<LoadEntry> loads; ///< 载荷/边界条件数据

  // =======================================================================
  // 便捷查询
  // =======================================================================

  /// @brief 是否包含单元连接性信息（区分点云 vs FEM 网格）
  bool hasElements() const { return !elements.empty(); }

  /// @brief 节点总数
  size_t numNodes() const { return nodeIDs.size(); }

  /// @brief 单元总数
  size_t numElements() const { return elemOffsets.size(); }

  /// @brief 清空所有数据
  void clear() {
    coords.clear();
    nodeIDs.clear();
    partIDs.clear();
    matIDs.clear();
    volumes.clear();
    elements.clear();
    elemOffsets.clear();
    elemTypes.clear();
    loads.clear();
  }
  // =======================================================================
  // 格式无关工具（从原 GrpdReader 迁入）
  // =======================================================================

  /// @brief 注册粒子基础几何场到 FieldManager (ID, PartID, MatID, Coords, Volume)
  static void
  ensureParticleFields(PDCommon::Field::FieldManager &fm);

  /// @brief 将 ParticleManager 中的粒子几何数据回填到 FieldManager
  static bool
  populateParticleFields(const PDCommon::Model::ParticleManager &pm,
                         PDCommon::Field::FieldManager &fm);
};

} // namespace PDCommon::IO

#endif // PDCOMMON_IO_MESH_DATA_H
