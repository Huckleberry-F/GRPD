#include "QuadCrack.h"
#include "PreCrackRegistry.h"
#include "Logger.h"
#include "FieldManager.h"
#include "ParticleManager.h"
#include "NeighborList.h"
#include <cmath>
#include <vector>

REGISTER_PRECRACK_MODEL(QuadCrack, PDCommon::Fracture::QuadCrack)

namespace PDCommon::Fracture {

void QuadCrack::configure(const YAML::Node &node) {
  auto readVec3 = [](const YAML::Node& n) -> Eigen::Vector3d {
      return Eigen::Vector3d(n[0].as<double>(), n[1].as<double>(), n[2].as<double>());
  };

  if (!node["V1"] || !node["V2"] || !node["V3"] || !node["V4"]) {
      LOG_ERROR("[QuadCrack] Missing vertex configurations (V1, V2, V3, V4)!");
      return;
  }

  v1_ = readVec3(node["V1"]);
  v2_ = readVec3(node["V2"]);
  v3_ = readVec3(node["V3"]);
  v4_ = readVec3(node["V4"]);

  // 计算法向量
  Eigen::Vector3d e1 = v2_ - v1_;
  Eigen::Vector3d e2 = v3_ - v2_;
  normal_ = e1.cross(e2);
  
  if (normal_.norm() < 1e-12) {
      LOG_ERROR("[QuadCrack] Invalid quad vertices: plane is degenerate.");
      normal_ = Eigen::Vector3d::UnitZ();
  } else {
      normal_.normalize();
  }

  LOG_INFO("[QuadCrack] Successfully configured QuadCrack with normal: (" +
           std::to_string(normal_.x()) + ", " +
           std::to_string(normal_.y()) + ", " +
           std::to_string(normal_.z()) + ")");
}

bool QuadCrack::doesIntersect(const Eigen::Vector3d &A, const Eigen::Vector3d &B) const {
  Eigen::Vector3d uA = A - v1_;
  Eigen::Vector3d uB = B - v1_;
  
  double dA = uA.dot(normal_);
  double dB = uB.dot(normal_);
  
  const double eps = 1e-9;
  // 严格同侧
  if ((dA > eps && dB > eps) || (dA < -eps && dB < -eps)) {
      return false; 
  }
  
  // 完全贴合在裂纹面（由于浮点误差，这种情况不应被剪断）
  if (std::abs(dA) < eps && std::abs(dB) < eps) {
      return false;
  }
  
  // 插值求交点 t 必定在 [0, 1] 内
  double denom = dA - dB;
  if(std::abs(denom) < eps) return false;

  double t = dA / denom;
  if (t < 0.0 || t > 1.0) {
      return false;
  }
  
  Eigen::Vector3d P = A + t * (B - A);
  
  // 利用四边形的有向边进行包络检查
  auto checkEdge = [&](const Eigen::Vector3d& start, const Eigen::Vector3d& end) {
      Eigen::Vector3d edge = end - start;
      Eigen::Vector3d toP = P - start;
      Eigen::Vector3d cp = edge.cross(toP);
      return cp.dot(normal_) >= -eps; // 取向一致则认为在内部
  };
  
  if (!checkEdge(v1_, v2_)) return false;
  if (!checkEdge(v2_, v3_)) return false;
  if (!checkEdge(v3_, v4_)) return false;
  if (!checkEdge(v4_, v1_)) return false;
  
  return true;
}

void QuadCrack::apply(PDCommon::Core::PDContext &ctx) {
  auto& manager = ctx.getParticleManager();
  auto& neighborList = ctx.getNeighborList();
  auto& fieldManager = ctx.getFieldManager();
  
  const size_t numParticles = manager.getTotalParticles();
  auto *coordsField = fieldManager.getFieldAs<double>("Coords");
  if (!coordsField) {
      LOG_ERROR("[QuadCrack] Coords field missing.");
      return;
  }
  const double* coords = coordsField->dataPtr();

  LOG_INFO("[QuadCrack] Evaluating bond breakage for crack...");

  int totalBondsBroken = 0;

  #pragma omp parallel for reduction(+:totalBondsBroken) schedule(guided)
  for (int i = 0; i < static_cast<int>(numParticles); ++i) {
      Eigen::Vector3d xi(coords[i * 3], coords[i * 3 + 1], coords[i * 3 + 2]);
      
      const int numNeighbors = neighborList.getNeighborCount(i);
      int* neighbors = neighborList.getNeighborIdsMutable(i);
      
      for (int k = 0; k < numNeighbors; ++k) {
          int j = neighbors[k];
          if (j == -1) continue;
          
          Eigen::Vector3d xj(coords[j * 3], coords[j * 3 + 1], coords[j * 3 + 2]);
          
          if (doesIntersect(xi, xj)) {
              neighbors[k] = -1;
              totalBondsBroken++;
          }
      }
  }

  LOG_INFO("[QuadCrack] Cut " + std::to_string(totalBondsBroken) + " bonds traversing the preset crack.");
}

} // namespace PDCommon::Fracture
