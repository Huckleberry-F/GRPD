#ifndef GRPD_IO_GRPD_READER_H
#define GRPD_IO_GRPD_READER_H

// ============================================================================
// GrpdReader.h - High-speed state-machine parser for .grpd files
// Responsibility: Read .grpd text files, fill *PARTICLE section data into
//                 ParticleManager, and apply *LOAD section data to physical
//                 field managers (e.g. ThermalField).
// ============================================================================

#include <functional>
#include <string>
#include <vector>

// 前向声明，避免循环头文件依赖
namespace GRPD::Model {
class ParticleManager;
class ThermalField;
} // namespace GRPD::Model

namespace GRPD::Field {
class FieldManager;
}

namespace GRPD::Core {
class PDSimulater;
} // namespace GRPD::Core

namespace GRPD::IO {

class GrpdReader {
public:
  // -----------------------------------------------------------------------
  // 读取 .grpd 文件的 *PARTICLE 段，填充粒子几何数据
  // @param filepath  .grpd 文件路径
  // @param manager   目标粒子管理器（引用传入）
  // @return true=成功, false=失败
  // -----------------------------------------------------------------------
  static bool read(const std::string &filepath,
                   GRPD::Model::ParticleManager &manager);

  /// @brief 注册粒子几何与标识字段到 FieldManager
  static void ensureParticleFields(GRPD::Field::FieldManager &fm);

  /// @brief 将 ParticleManager 中的粒子几何与标识数据回填到 FieldManager
  static bool populateParticleFields(const GRPD::Model::ParticleManager &pm,
                                     GRPD::Field::FieldManager &fm);

  // -----------------------------------------------------------------------
  // 读取 .grpd 文件的 *LOAD 段，将温度型载荷施加到 ThermalField
  // @param filepath   .grpd 文件路径
  // @param simulater  仿真大管家（用于获取 BCManager 和 ThermalField）
  // @return true=成功, false=失败
  // -----------------------------------------------------------------------
  static bool readLoads(const std::string &filepath,
                        GRPD::Core::PDSimulater &simulater);

private:
  /// 内部解析状态枚举
  enum class ParseState {
    IDLE,              // 初始状态，等待段标记
    READING_PARTICLES, // 正在读取 *PARTICLE 段
    READING_LOADS      // 正在读取 *LOAD 段
  };

  // -----------------------------------------------------------------------
  // 行回调函数签名：(当前状态, 已分割的字段列表, 行号)
  // 返回 true 表示继续扫描，false 表示终止
  // -----------------------------------------------------------------------
  using LineCallback = std::function<bool(ParseState state,
                                          const std::vector<std::string> &tokens,
                                          int lineNumber)>;

  // -----------------------------------------------------------------------
  // 通用文件扫描器：封装文件打开、逐行读取、状态机切换
  // @param filepath    .grpd 文件路径
  // @param callback    每遇到有效数据行时调用的回调
  // @param logPrefix   日志前缀标识（用于区分调用来源）
  // @return true=成功, false=文件打开失败
  // -----------------------------------------------------------------------
  static bool scanFile(const std::string &filepath,
                       LineCallback callback,
                       const std::string &logPrefix);

  /// 将逗号分隔的一行文本分割为修剪后的字段数组
  static std::vector<std::string> tokenizeCsv(const std::string &line);

  /// 去除字符串首尾空白字符
  static std::string trim(const std::string &s);
};

} // namespace GRPD::IO

#endif // GRPD_IO_GRPD_READER_H
