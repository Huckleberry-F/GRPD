#ifndef GRPD_UTILS_DATAEXTRACTOR_H
#define GRPD_UTILS_DATAEXTRACTOR_H

#include <array>
#include <vector>

namespace GRPD::Utils {

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

} // namespace GRPD::Utils

#endif // GRPD_UTILS_DATAEXTRACTOR_H
