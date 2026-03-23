#ifndef PDCOMMON_IO_READER_MANAGER_H
#define PDCOMMON_IO_READER_MANAGER_H

// ============================================================================
// ReaderManager.h - 网格读取器实例管理器 (非单例，每个模型一个)
//
// 架构对称于 MaterialManager：
//   MaterialManager: 持有模型中所有 Material 实例
//   ReaderManager:   持有模型中所有 MeshReader 实例
//
// 非单例设计：由上层 PDContext 或 Initializer 持有，
// 支持同一模型中加载多种格式的网格文件。
// ============================================================================

#include "MeshReader.h"
#include <map>
#include <memory>
#include <string>

namespace PDCommon::IO {

class ReaderManager {
public:
  ReaderManager() = default;
  ~ReaderManager() = default;

  // 禁用拷贝
  ReaderManager(const ReaderManager &) = delete;
  ReaderManager &operator=(const ReaderManager &) = delete;

  // 允许移动
  ReaderManager(ReaderManager &&) = default;
  ReaderManager &operator=(ReaderManager &&) = default;

  // -----------------------------------------------------------------------
  // 读取器实例管理
  // -----------------------------------------------------------------------

  /// @brief 添加一个已创建好的读取器实例
  /// @param name 读取器实例名称（如 "MainMesh", "BoundaryMesh"）
  /// @param reader 读取器实例的所有权（unique_ptr）
  void addReader(const std::string &name,
                 std::unique_ptr<MeshReader> reader);

  /// @brief 获取已实例化的读取器指针
  /// @param name 读取器实例名称
  MeshReader *getReader(const std::string &name);
  const MeshReader *getReader(const std::string &name) const;

  /// @brief 检查读取器实例是否存在
  bool hasReader(const std::string &name) const;

  /// @brief 获取已注册读取器总数
  size_t getReaderCount() const { return readers_.size(); }

  /// @brief 获取所有读取器的引用（用于遍历）
  const std::map<std::string, std::unique_ptr<MeshReader>> &
  getReaders() const {
    return readers_;
  }

private:
  /// 存储当前上下文中的所有读取器实例（实例名 -> unique_ptr<MeshReader>）
  std::map<std::string, std::unique_ptr<MeshReader>> readers_;
};

} // namespace PDCommon::IO

#endif // PDCOMMON_IO_READER_MANAGER_H
