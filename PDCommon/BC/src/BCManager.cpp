// ============================================================================
// BCManager.cpp - 边界条件实例管理器实现（纯容器）
// ============================================================================

#include "BCManager.h"

namespace PDCommon::BC {

void BCManager::addBC(std::unique_ptr<BC> bc, int step) {
    bcs_.push_back({step, std::move(bc)});
}

size_t BCManager::size() const { return bcs_.size(); }

void BCManager::clear() { bcs_.clear(); }

} // namespace PDCommon::BC
