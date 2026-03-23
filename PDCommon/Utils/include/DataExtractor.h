#ifndef PDCOMMON_UTILS_DATAEXTRACTOR_H
#define PDCOMMON_UTILS_DATAEXTRACTOR_H

// ==========================================================================
// [遗留模块 - 可删除]
// 本模块已被 FieldManager + TypedField 体系完全替代。
// 所有物理场数据现在通过 FieldManager 统一管理，不再需要从 Particle 结构体中
// 逐字段提取裸指针。当前仅 ParticleManager.cpp 残留引用，但其对外 getter
// (getCoordsPtr 等) 已无任何外部调用者。待确认后可安全移除本文件。
// ==========================================================================

#include <array>
#include <vector>

namespace PDCommon::Utils {

class DataExtractor {
public:
  // Extract a scalar property into a continuous array
  // MemberPtr as template arg ensures a unique thread_local buffer per property
  template <typename C, typename T, T C::*MemberPtr>
  static T *getScalarPtr(const std::vector<C> &container) {
    // Use thread_local to avoid repeated heavy allocations in the main thread
    thread_local std::vector<T> buf;
    buf.resize(container.size());
    for (size_t i = 0; i < container.size(); ++i) {
      buf[i] = container[i].*MemberPtr;
    }
    return buf.data();
  }

  // Extract an std::array vector property (e.g. 3D coordinates) into a
  // continuous array
  template <typename C, typename T, size_t N, std::array<T, N> C::*MemberPtr>
  static T *getVectorPtr(const std::vector<C> &container) {
    thread_local std::vector<T> buf;
    buf.resize(container.size() * N);
    for (size_t i = 0; i < container.size(); ++i) {
      const auto &arr = container[i].*MemberPtr;
      for (size_t j = 0; j < N; ++j) {
        buf[i * N + j] = arr[j];
      }
    }
    return buf.data();
  }
};

} // namespace PDCommon::Utils

#endif // PDCOMMON_UTILS_DATAEXTRACTOR_H
