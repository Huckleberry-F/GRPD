#ifndef PDCOMMON_IO_GRPD_READER_H
#define PDCOMMON_IO_GRPD_READER_H

// ============================================================================
// GrpdReader.h - High-speed state-machine parser for .grpd files
// Responsibility: Read .grpd text files, fill *PARTICLE section data into
//                 ParticleManager, and apply *LOAD section data to physical
//                 field managers (e.g. ThermalField).
// ============================================================================

#include <functional>
#include <string>
#include <vector>

// 鍓嶅悜澹版槑锛岄伩鍏嶅惊鐜ご鏂囦欢渚濊禆
namespace PDCommon::Model {
class ParticleManager;
class ThermalField;
} // namespace PDCommon::Model

namespace PDCommon::Field {
class FieldManager;
}

namespace PDCommon::Core {
class PDContext;
} // namespace PDCommon::Core

namespace PDCommon::IO {

class GrpdReader {
public:
  // -----------------------------------------------------------------------
  // 璇诲彇 .grpd 鏂囦欢鐨?*PARTICLE 娈碉紝濉厖绮掑瓙鍑犱綍鏁版嵁
  // @param filepath  .grpd 鏂囦欢璺緞
  // @param manager   鐩爣绮掑瓙绠＄悊鍣紙寮曠敤浼犲叆锛?
  // @return true=鎴愬姛, false=澶辫触
  // -----------------------------------------------------------------------
  static bool read(const std::string &filepath,
                   PDCommon::Model::ParticleManager &manager);

  /// @brief 娉ㄥ唽绮掑瓙鍑犱綍涓庢爣璇嗗瓧娈靛埌 FieldManager
  static void ensureParticleFields(PDCommon::Field::FieldManager &fm);

  /// @brief 灏?ParticleManager 涓殑绮掑瓙鍑犱綍涓庢爣璇嗘暟鎹洖濉埌 FieldManager
  static bool populateParticleFields(const PDCommon::Model::ParticleManager &pm,
                                     PDCommon::Field::FieldManager &fm);

  // -----------------------------------------------------------------------
  // 璇诲彇 .grpd 鏂囦欢鐨?*LOAD 娈碉紝灏嗘俯搴﹀瀷杞借嵎鏂藉姞鍒?ThermalField
  // @param filepath   .grpd 鏂囦欢璺緞
  // @param simulater  浠跨湡澶х瀹讹紙鐢ㄤ簬鑾峰彇 BCManager 鍜?ThermalField锛?
  // @return true=鎴愬姛, false=澶辫触
  // -----------------------------------------------------------------------
  static bool readLoads(const std::string &filepath,
                        PDCommon::Core::PDContext &simulater);

private:
  /// 鍐呴儴瑙ｆ瀽鐘舵€佹灇涓?
  enum class ParseState {
    IDLE,              // 鍒濆鐘舵€侊紝绛夊緟娈垫爣璁?
    READING_PARTICLES, // 姝ｅ湪璇诲彇 *PARTICLE 娈?
    READING_LOADS      // 姝ｅ湪璇诲彇 *LOAD 娈?
  };

  // -----------------------------------------------------------------------
  // 琛屽洖璋冨嚱鏁扮鍚嶏細(褰撳墠鐘舵€? 宸插垎鍓茬殑瀛楁鍒楄〃, 琛屽彿)
  // 杩斿洖 true 琛ㄧず缁х画鎵弿锛宖alse 琛ㄧず缁堟
  // -----------------------------------------------------------------------
  using LineCallback = std::function<bool(ParseState state,
                                          const std::vector<std::string> &tokens,
                                          int lineNumber)>;

  // -----------------------------------------------------------------------
  // 閫氱敤鏂囦欢鎵弿鍣細灏佽鏂囦欢鎵撳紑銆侀€愯璇诲彇銆佺姸鎬佹満鍒囨崲
  // @param filepath    .grpd 鏂囦欢璺緞
  // @param callback    姣忛亣鍒版湁鏁堟暟鎹鏃惰皟鐢ㄧ殑鍥炶皟
  // @param logPrefix   鏃ュ織鍓嶇紑鏍囪瘑锛堢敤浜庡尯鍒嗚皟鐢ㄦ潵婧愶級
  // @return true=鎴愬姛, false=鏂囦欢鎵撳紑澶辫触
  // -----------------------------------------------------------------------
  static bool scanFile(const std::string &filepath,
                       LineCallback callback,
                       const std::string &logPrefix);

  /// 灏嗛€楀彿鍒嗛殧鐨勪竴琛屾枃鏈垎鍓蹭负淇壀鍚庣殑瀛楁鏁扮粍
  static std::vector<std::string> tokenizeCsv(const std::string &line);

  /// 鍘婚櫎瀛楃涓查灏剧┖鐧藉瓧绗?
  static std::string trim(const std::string &s);
};

} // namespace PDCommon::IO

#endif // PDCOMMON_IO_GRPD_READER_H
