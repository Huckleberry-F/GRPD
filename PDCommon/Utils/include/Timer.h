// ============================================================================
// Timer.h - 轻量级高精度计时工具（Header-Only）
// ============================================================================
// 用途：统一积分器中散落的 std::chrono 计时逻辑，提供简洁的
//       tick/tock 接口与自动化的统计输出能力。
// ============================================================================

#ifndef PDCOMMON_UTILS_TIMER_H
#define PDCOMMON_UTILS_TIMER_H

#include "Logger.h"
#include <chrono>
#include <string>

namespace PDCommon::Utils {

/// @brief 高精度计时器，封装 std::chrono::high_resolution_clock
class Timer {
public:
  using Clock = std::chrono::high_resolution_clock;
  using TimePoint = Clock::time_point;

  /// @brief 记录全局起点（在主循环开始前调用一次）
  inline void start() {
    globalStart_ = Clock::now();
    pureComputeTime_ = 0.0;
    totalTicks_ = 0;
  }

  /// @brief 单步计时开始（在每个计算段之前调用）
  inline void tick() { tickStart_ = Clock::now(); }

  /// @brief 单步计时结束（在每个计算段之后调用），自动累加纯计算时间
  inline void tock() {
    auto now = Clock::now();
    pureComputeTime_ +=
        std::chrono::duration<double>(now - tickStart_).count();
    totalTicks_++;
  }

  /// @brief 返回已累计的 tick 次数
  inline long long totalTicks() const { return totalTicks_; }

  /// @brief 返回纯计算总耗时（秒）
  inline double pureComputeTime() const { return pureComputeTime_; }

  /// @brief 返回从 start() 至今的挂钟耗时（秒）
  inline double totalElapsed() const {
    return std::chrono::duration<double>(Clock::now() - globalStart_).count();
  }

  /// @brief 返回平均计算速度（steps/s），若无有效数据返回 0
  inline double pureSpeed() const {
    return (pureComputeTime_ > 0.0) ? (totalTicks_ / pureComputeTime_) : 0.0;
  }

  /// @brief 返回每步平均耗时（s/step），若无有效数据返回 0
  inline double avgStepTime() const {
    return (totalTicks_ > 0) ? (pureComputeTime_ / totalTicks_) : 0.0;
  }

  /// @brief 输出结束统计日志
  /// @param prefix 积分器名称前缀，如 "CentralDifference"
  inline void logSummary(const std::string &prefix) const {
    LOG_INFO("[" + prefix + "] Finished. Total: " +
             std::to_string(totalElapsed()) +
             "s  |  Pure Compute avg: " + std::to_string(avgStepTime()) +
             " s/step");
  }

private:
  TimePoint globalStart_{};   ///< 全局起始时刻
  TimePoint tickStart_{};     ///< 当前 tick 起始时刻
  double pureComputeTime_{0}; ///< 纯计算累计时间（秒）
  long long totalTicks_{0};   ///< 累计 tick 次数
};

} // namespace PDCommon::Utils

#endif // PDCOMMON_UTILS_TIMER_H
