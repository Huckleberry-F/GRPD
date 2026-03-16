// ============================================================================
// PDContext.cpp - 杩戝満鍔ㄥ姏瀛︿豢鐪熸€绘帶绫诲疄鐜?
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
  neighborList_ = std::make_unique<PDCommon::Initial::NeighborList>();
  neighborList_->buildNeighbors(mgr, horizon);
}

} // namespace PDCommon::Core
