// ============================================================================
// BCManager.cpp - 边界条件实例管理器实现
// ============================================================================

#include "BCManager.h"

namespace PDCommon::BC {

void BCManager::addBC(std::unique_ptr<BC> bc, int step) {
    bcs_.push_back({step, std::move(bc)});
}

void BCManager::applySources(double loadFactor, int currentStep) {
    for (auto &entry : bcs_) {
        if (entry.step == 0 || entry.step == currentStep) {
            if (!entry.bc->isConstraint()) {
                entry.bc->apply(loadFactor);
            }
        }
    }
}

void BCManager::applyConstraints(double loadFactor, int currentStep) {
    for (auto &entry : bcs_) {
        if (entry.step == 0 || entry.step == currentStep) {
            if (entry.bc->isConstraint()) {
                entry.bc->apply(loadFactor); // ADR 等积分器依赖的原生 loadFactor 放缩
            }
        }
    }
}

void BCManager::commitEndStep() {
    for (auto &entry : bcs_) {
        entry.bc->commitEndStep();
    }
}

size_t BCManager::size() const { return bcs_.size(); }

void BCManager::clear() { bcs_.clear(); }

} // namespace PDCommon::BC
