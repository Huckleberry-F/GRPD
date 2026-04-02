// ============================================================================
// StringUtils.h - 字符串与日志输出辅助工具
// ============================================================================

#ifndef PDCOMMON_UTILS_STRING_UTILS_H
#define PDCOMMON_UTILS_STRING_UTILS_H

#include <iomanip>
#include <sstream>
#include <string>

namespace PDCommon::Utils {

class StringUtils {
public:
  /// @brief 将双精度浮点数格式化为科学计数法字符串
  /// @param val 数值
  /// @param precision 保留的小数位数，默认 4 位
  /// @return 科学计数法格式字符串（如 "1.2345e-04"）
  static inline std::string toScientific(double val, int precision = 4) {
    std::ostringstream oss;
    oss << std::scientific << std::setprecision(precision) << val;
    return oss.str();
  }
};

} // namespace PDCommon::Utils

#endif // PDCOMMON_UTILS_STRING_UTILS_H
