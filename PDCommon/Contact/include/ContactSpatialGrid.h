#ifndef PDCOMMON_CONTACT_CONTACTSPATIALGRID_H
#define PDCOMMON_CONTACT_CONTACTSPATIALGRID_H

#include <Eigen/Dense>
#include <vector>

namespace PDCommon::Contact {

class ContactSpatialGrid {
public:
  ContactSpatialGrid() = default;
  ~ContactSpatialGrid() = default;

  void build(const std::vector<int> &masterIds, const double *coords,
             const double *disp, const int *activeStatus, double maxDx,
             size_t numParticles);

  double getCellSize() const { return cellSize_; }
  const Eigen::Vector3d &getMinBounds() const { return minBounds_; }
  const Eigen::Vector3i &getGridDims() const { return gridDims_; }
  const std::vector<int> &getHead() const { return head_; }
  const std::vector<int> &getNext() const { return next_; }

  int computeCellHash(double x, double y, double z) const;

  /// @brief Iterates over potential neighbors in the 27 grid cells around (x, y, z).
  /// @param callback A callable or lambda function taking an `int neighborId` and returning `bool` (true to continue, false to early exit).
  template <typename Func>
  void forEachNeighbor(double x, double y, double z, Func callback) const {
    if (head_.empty()) return;
    int cx = static_cast<int>(std::floor((x - minBounds_.x()) / cellSize_));
    int cy = static_cast<int>(std::floor((y - minBounds_.y()) / cellSize_));
    int cz = static_cast<int>(std::floor((z - minBounds_.z()) / cellSize_));

    for (int offset_x = -1; offset_x <= 1; ++offset_x) {
      for (int offset_y = -1; offset_y <= 1; ++offset_y) {
        for (int offset_z = -1; offset_z <= 1; ++offset_z) {
          int ncx = cx + offset_x, ncy = cy + offset_y, ncz = cz + offset_z;
          if (ncx < 0 || ncx >= gridDims_.x() || 
              ncy < 0 || ncy >= gridDims_.y() || 
              ncz < 0 || ncz >= gridDims_.z())
            continue;

          int hash = ncx + ncy * gridDims_.x() + ncz * gridDims_.x() * gridDims_.y();
          int j = head_[hash];

          while (j != -1) {
            callback(j);
            j = next_[j];
          }
        }
      }
    }
  }

public:
  double cellSize_ = 0.0;
  Eigen::Vector3d minBounds_;
  Eigen::Vector3d maxBounds_;
  Eigen::Vector3i gridDims_;
  std::vector<int> head_;
  std::vector<int> next_;
};

} // namespace PDCommon::Contact

#endif // PDCOMMON_CONTACT_CONTACTSPATIALGRID_H
