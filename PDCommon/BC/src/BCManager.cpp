// ============================================================================
// BCManager.cpp - 边界条件实例管理器实现
// ============================================================================

#include "BCManager.h"

namespace GRPD::BC {

void BCManager::addBC(std::unique_ptr<BC> bc) {
    bcs_.push_back(std::move(bc));
}

void BCManager::applyAll() {
    for (auto &bc : bcs_) {
        bc->apply();
    }
}

void BCManager::applySources() {
    for (auto &bc : bcs_) {
        if (!bc->isConstraint()) {
            bc->apply();
        }
    }
}

void BCManager::applyConstraints() {
    for (auto &bc : bcs_) {
        if (bc->isConstraint()) {
            bc->apply();
        }
    }
}

size_t BCManager::size() const { return bcs_.size(); }

void BCManager::clear() { bcs_.clear(); }

} // namespace GRPD::BC
