#ifndef PDCOMMON_IO_GRPD_MESH_READER_H
#define PDCOMMON_IO_GRPD_MESH_READER_H

// ============================================================================
// GrpdMeshReader.h - .grpd 格式读取器（完整实现）
//
// 吸收了原 GrpdReader 的全部解析逻辑：
//   - 状态机逐行扫描（trim / tokenizeCsv / scanFile）
//   - *PARTICLE 段 → meshData_ 几何字段
//   - *LOAD 段    → meshData_.loads 边界条件
//
// 架构地位：MeshReader 的具体派生类，通过 REGISTER_READER 宏自动注册。
// ============================================================================

#include "MeshReader.h"
#include <functional>
#include <string>
#include <vector>

namespace PDCommon::IO {

class GrpdMeshReader : public MeshReader {
public:
  /// @brief 构造函数
  explicit GrpdMeshReader(const std::string &name = "");

  ~GrpdMeshReader() override = default;

  /// @brief 读取 .grpd 文件，同时解析 *PARTICLE 和 *LOAD 段
  bool read(const std::string &filepath) override;

private:
  /// 内部解析状态枚举
  enum class ParseState {
    IDLE,              ///< 初始状态，等待段标记
    READING_PARTICLES, ///< 正在读取 *PARTICLE 段
    READING_LOADS      ///< 正在读取 *LOAD 段
  };

  /// 行回调函数签名
  using LineCallback = std::function<bool(ParseState state,
                                          const std::vector<std::string> &tokens,
                                          int lineNumber)>;

  /// @brief 去除字符串首尾空白
  static std::string trim(const std::string &s);

  /// @brief 将逗号分隔的一行文本分割为修剪后的字段数组
  static std::vector<std::string> tokenizeCsv(const std::string &line);

  /// @brief 通用文件扫描器：封装文件打开、逐行读取、状态机切换
  static bool scanFile(const std::string &filepath, LineCallback callback,
                       const std::string &logPrefix);
};

} // namespace PDCommon::IO

#endif // PDCOMMON_IO_GRPD_MESH_READER_H
