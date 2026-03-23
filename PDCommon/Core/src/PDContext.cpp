// ============================================================================
// PDContext.cpp - 鏉╂垵婧€閸斻劌濮忕€涳缚璞㈤惇鐔糕偓缁樺付缁鐤勯悳?
// ============================================================================

#include "PDContext.h"
#include "Logger.h"
#include <string>

namespace PDCommon::Core {

PDContext::PDContext(const std::string &name) : name_(name) {
  LOG_INFO(std::string("[PDContext] Start: ") + name_);
}

void PDContext::createNeighborList(const PDCommon::Model::ParticleManager &mgr,
                                    double horizon) {
  neighborList_ = std::make_unique<PDCommon::Neighbor::NeighborList>();
  neighborList_->buildNeighbors(mgr, horizon);
}

} // namespace PDCommon::Core
