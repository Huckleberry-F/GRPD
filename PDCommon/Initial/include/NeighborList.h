#ifndef PDCOMMON_INITIAL_NEIGHBOR_LIST_H
#define PDCOMMON_INITIAL_NEIGHBOR_LIST_H

// ============================================================================
// NeighborList.h - 近场动力学近邻搜索管理器
// 责任：
// 1. 负责构建并存储粒子的邻域列表 (Horizon)
// 2. 提供高效的邻居遍历接口
// 架构规约：属于 Initial 模块，负责仿真前的预处理
// ============================================================================

#include <vector>
#include <cstddef>

namespace PDCommon::Model {
    class ParticleManager;
}

namespace PDCommon::Initial {

class NeighborList {
public:
    NeighborList() = default;
    ~NeighborList() = default;

    // 禁用拷贝
    NeighborList(const NeighborList&) = delete;
    NeighborList& operator=(const NeighborList&) = delete;

    /**
     * @brief 构建近邻列表 (暴力搜索版本 O(N^2))
     * @param mgr ParticleManager 实例，包含所有点位置
     * @param horizon 邻域半径 delta
     */
    void buildNeighbors(const PDCommon::Model::ParticleManager& mgr, double horizon);

    /**
     * @brief 获取指定粒子的邻居 ID 列表
     * @param particleId 粒子全局索引
     * @return const std::vector<int>& 邻居 ID 数组
     */
    const std::vector<int>& getNeighbors(int particleId) const;

    /**
     * @brief 获取总粒子数（也就是列表的外层尺寸）
     */
    size_t size() const;

    /**
     * @brief 获取构建时使用的邻域半径 delta
     */
    double getHorizon() const;

    /**
     * @brief 获取指定粒子的邻居数量
     */
    size_t getNeighborCount(int particleId) const;

    /**
     * @brief 清空数据
     */
    void clear();

private:
    // 外层向量索引为粒子 ID，内层向量存储该粒子的所有邻居 ID
    std::vector<std::vector<int>> neighbors_;
    double horizon_{0.0}; // 邻域半径
};

} // namespace PDCommon::Initial

#endif // PDCOMMON_INITIAL_NEIGHBOR_LIST_H
