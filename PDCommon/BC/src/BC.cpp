// ============================================================================
// BC.cpp - 边界条件基类实现
// ============================================================================

#include "BC.h"
#include "BCManager.h"

namespace PDCommon::BC {

BC::BC(const std::string &name) : name_(name) {}

const std::string &BC::getName() const { return name_; }

void BC::setName(const std::string &name) { name_ = name; }

// ---------------------------------------------------------------------------
// 静态调度辅助函数实现（从 BCManager 获取 entries）
// ---------------------------------------------------------------------------

void BC::applySources(BCManager &mgr, double loadFactor, int currentStep) {
  for (auto &entry : mgr.getBCs()) {
    if ((entry.step == 0 || entry.step == currentStep) && !entry.bc->isConstraint()) {
      entry.bc->apply(loadFactor);
    }
  }
}

void BC::applyConstraints(BCManager &mgr, double loadFactor, int currentStep) {
  for (auto &entry : mgr.getBCs()) {
    if ((entry.step == 0 || entry.step == currentStep) && entry.bc->isConstraint()) {
      entry.bc->apply(loadFactor);
    }
  }
}

void BC::commitEndStep(BCManager &mgr, int currentStep) {
  for (auto &entry : mgr.getBCs()) {
    if (entry.step == 0 || entry.step == currentStep) {
      entry.bc->commitEndStep();
    }
  }
}

} // namespace PDCommon::BC
