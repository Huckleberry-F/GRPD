#ifndef PDCOMMON_UTILS_MATH_UTILS_H
#define PDCOMMON_UTILS_MATH_UTILS_H

#include <cmath>

namespace PDCommon::Utils {

/**
 * @brief 安全除法，防止除以零或绝对值极小值引起的 NaN 或溢出崩溃。
 * @param numerator 分子
 * @param denominator 分母
 * @param fallback 分母接近零时的返回值，默认为 0.0
 * @param epsilon 容差阈值，分母绝对值小于该值视为除零，默认为 1e-15
 * @return 正常的商或 fallback 值
 */
inline double safeDivide(double numerator, double denominator, double fallback = 0.0, double epsilon = 1e-15) {
    if (std::abs(denominator) < epsilon) {
        return fallback;
    }
    return numerator / denominator;
}

} // namespace PDCommon::Utils

#endif // PDCOMMON_UTILS_MATH_UTILS_H
