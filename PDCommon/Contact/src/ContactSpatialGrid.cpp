#include "ContactSpatialGrid.h"
#include "StringUtils.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace PDCommon::Contact {

void ContactSpatialGrid::build(const std::vector<int> &masterIds,
                               const double *coords, const double *disp,
                               const int *activeStatus, double maxDx,
                               size_t numParticles) {
  cellSize_ = maxDx * 1.05;
  minBounds_ = Eigen::Vector3d(1e10, 1e10, 1e10);
  maxBounds_ = Eigen::Vector3d(-1e10, -1e10, -1e10);

  for (int i : masterIds) {
    if (activeStatus && activeStatus[i] == 0)
      continue;
    double cur_x = coords[i * 3] + (disp ? disp[i * 3] : 0.0);
    double cur_y = coords[i * 3 + 1] + (disp ? disp[i * 3 + 1] : 0.0);
    double cur_z = coords[i * 3 + 2] + (disp ? disp[i * 3 + 2] : 0.0);
    minBounds_.x() = std::min(minBounds_.x(), cur_x);
    minBounds_.y() = std::min(minBounds_.y(), cur_y);
    minBounds_.z() = std::min(minBounds_.z(), cur_z);
    maxBounds_.x() = std::max(maxBounds_.x(), cur_x);
    maxBounds_.y() = std::max(maxBounds_.y(), cur_y);
    maxBounds_.z() = std::max(maxBounds_.z(), cur_z);
  }

  Eigen::Vector3d range = maxBounds_ - minBounds_;
  if (range.x() < cellSize_) {
    minBounds_.x() -= cellSize_;
    maxBounds_.x() += cellSize_;
  }
  if (range.y() < cellSize_) {
    minBounds_.y() -= cellSize_;
    maxBounds_.y() += cellSize_;
  }
  if (range.z() < cellSize_) {
    minBounds_.z() -= cellSize_;
    maxBounds_.z() += cellSize_;
  }

  if (std::isnan(minBounds_.x()) || std::isnan(maxBounds_.x()) ||
      std::isnan(minBounds_.y()) || std::isnan(maxBounds_.y()) ||
      std::isnan(minBounds_.z()) || std::isnan(maxBounds_.z())) {
    throw std::runtime_error("[ContactSpatialGrid] Bounds contain NaN! Simulation exploded.");
  }

  if (maxBounds_.x() - minBounds_.x() > 1e6 || maxBounds_.y() - minBounds_.y() > 1e6 || maxBounds_.z() - minBounds_.z() > 1e6) {
    throw std::runtime_error("[ContactSpatialGrid] Bounding box is excessively large! Simulation divergence detected.");
  }

  gridDims_.x() = static_cast<int>(std::ceil((maxBounds_.x() - minBounds_.x()) / cellSize_));
  gridDims_.y() = static_cast<int>(std::ceil((maxBounds_.y() - minBounds_.y()) / cellSize_));
  gridDims_.z() = static_cast<int>(std::ceil((maxBounds_.z() - minBounds_.z()) / cellSize_));

  if (gridDims_.x() <= 0 || gridDims_.y() <= 0 || gridDims_.z() <= 0) {
    throw std::runtime_error("[ContactSpatialGrid] Grid Dims evaluated to <= 0 !");
  }

  size_t numCells = static_cast<size_t>(gridDims_.x()) * gridDims_.y() * gridDims_.z();
      
  if (numCells > 1e8) {
    throw std::runtime_error("[ContactSpatialGrid] Cell grid allocation exceeded safe limits (" + PDCommon::Utils::StringUtils::toScientific(static_cast<double>(numCells)) + " > 1e8).");
  }

  head_.assign(numCells, -1);

  if (next_.size() < numParticles)
    next_.resize(numParticles, -1);

  for (int i : masterIds) {
    if (activeStatus && activeStatus[i] == 0)
      continue;
    double cur_x = coords[i * 3] + (disp ? disp[i * 3] : 0.0);
    double cur_y = coords[i * 3 + 1] + (disp ? disp[i * 3 + 1] : 0.0);
    double cur_z = coords[i * 3 + 2] + (disp ? disp[i * 3 + 2] : 0.0);
    int cellHash = computeCellHash(cur_x, cur_y, cur_z);
    next_[i] = head_[cellHash];
    head_[cellHash] = static_cast<int>(i);
  }
}

int ContactSpatialGrid::computeCellHash(double x, double y, double z) const {
  int cx = static_cast<int>(std::floor((x - minBounds_.x()) / cellSize_));
  int cy = static_cast<int>(std::floor((y - minBounds_.y()) / cellSize_));
  int cz = static_cast<int>(std::floor((z - minBounds_.z()) / cellSize_));
  cx = std::max(0, std::min(cx, gridDims_.x() - 1));
  cy = std::max(0, std::min(cy, gridDims_.y() - 1));
  cz = std::max(0, std::min(cz, gridDims_.z() - 1));
  return cx + cy * gridDims_.x() + cz * gridDims_.x() * gridDims_.y();
}

} // namespace PDCommon::Contact
