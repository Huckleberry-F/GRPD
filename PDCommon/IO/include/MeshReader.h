#ifndef PDCOMMON_IO_MESH_READER_H
#define PDCOMMON_IO_MESH_READER_H

// ============================================================================
// MeshReader.h - 网格文件读取器抽象基类
//
// 架构对称于 Material 基类：
//   Material:   name_, matId_, initialize(YAML), evaluate() = 0
//   MeshReader: name_,         initialize(YAML), read(filepath) = 0
//
// 所有具体解析器（GrpdMeshReader, InpMeshReader 等）继承此基类，
// 只需实现 read() 方法将文件内容填入 meshData_ 即可。
// 上层代码通过 getMeshData() 统一消费，完全不关心原始文件格式。
// ============================================================================

#include "MeshData.h"
#include <string>
#include <yaml-cpp/yaml.h>

namespace PDCommon::IO {

class MeshReader {
public:
  /// @brief 构造函数
  /// @param name 读取器实例的名称（对称 Material(name)）
  explicit MeshReader(const std::string &name = "");

  virtual ~MeshReader() = default;

  // 禁用拷贝构造和赋值（对称 Material）
  MeshReader(const MeshReader &) = delete;
  MeshReader &operator=(const MeshReader &) = delete;

  /// @brief 获取读取器实例名称
  const std::string &getName() const;

  /// @brief 设置名称
  void setName(const std::string &name);

  /// @brief 初始化读取器参数（可选，子类按需覆盖）
  /// @param config 对应的 YAML 配置节点
  virtual void initialize(const YAML::Node &config);

  /// @brief 读取网格文件，将解析结果填入内部 meshData_
  /// @param filepath 文件路径
  /// @return true=成功, false=失败
  virtual bool read(const std::string &filepath) = 0;

  /// @brief 获取解析后的通用网格数据（只读引用）
  const MeshData &getMeshData() const { return meshData_; }

  /// @brief 获取解析后的通用网格数据（可写引用，供子类使用）
  MeshData &getMeshData() { return meshData_; }

protected:
  std::string name_;  ///< 读取器实例名称（对称 Material::name_）
  MeshData meshData_; ///< 通用中间容器（对称 Writer 的 pointsVariable）
};

} // namespace PDCommon::IO

#endif // PDCOMMON_IO_MESH_READER_H
