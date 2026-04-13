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
