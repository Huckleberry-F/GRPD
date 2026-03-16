#include "NeighborList.h"
#include "ParticleManager.h"
#include "Logger.h"
#include <cmath>
#include <string>
#include <omp.h>

namespace PDCommon::Initial {

void NeighborList::buildNeighbors(const PDCommon::Model::ParticleManager& mgr, double horizon) {
    this->horizon_ = horizon;
    size_t numParticles = mgr.getTotalParticles();
    LOG_INFO("[NeighborList] Starting neighbor search for " + std::to_string(numParticles) + 
             " particles with horizon = " + std::to_string(horizon));

    // 1. 初始化列表
    neighbors_.assign(numParticles, std::vector<int>());
    double h2 = horizon * horizon;

    // 2. 双重循环暴力搜索 (并利用 OpenMP 加速)
    // 注意：PD 键是对称的，但为了代码简单和避免复杂的 atomic，先使用全量遍历
    // 在 12w 规模下，并行化非常重要
    #pragma omp parallel for schedule(dynamic, 64)
    for (int i = 0; i < static_cast<int>(numParticles); ++i) {
        const auto& p_i = mgr.getParticle(i);
        const auto& pos_i = p_i.getCoords();

        // 预分配内层空间可以减少内存重分配开销 (估算 PD 邻居数约为 50-100)
        neighbors_[i].reserve(100);

        for (int j = 0; j < static_cast<int>(numParticles); ++j) {
            if (i == j) continue;

            const auto& p_j = mgr.getParticle(j);
            const auto& pos_j = p_j.getCoords();

            double dx = pos_j[0] - pos_i[0];
            double dy = pos_j[1] - pos_i[1];
            double dz = pos_j[2] - pos_i[2];
            double dist2 = dx*dx + dy*dy + dz*dz;

            if (dist2 <= h2) {
                neighbors_[i].push_back(j);
            }
        }
    }

    size_t totalBonds = 0;
    for (const auto& list : neighbors_) {
        totalBonds += list.size();
    }

    LOG_INFO("[NeighborList] Neighbor search complete. Total bonds found: " + 
             std::to_string(totalBonds));
}

const std::vector<int>& NeighborList::getNeighbors(int particleId) const {
    return neighbors_[particleId];
}

size_t NeighborList::size() const {
    return neighbors_.size();
}

size_t NeighborList::getNeighborCount(int particleId) const {
    return neighbors_[particleId].size();
}

void NeighborList::clear() {
    neighbors_.clear();
}

double NeighborList::getHorizon() const {
    return horizon_;
}

} // namespace PDCommon::Initial
