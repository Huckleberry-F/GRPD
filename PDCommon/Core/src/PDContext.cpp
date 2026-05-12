// ============================================================================
// PDContext.cpp - Context Implementation
// ============================================================================

#include "PDContext.h"
#include "Logger.h"
#include "PostProcessorManager.h"
#include <string>

namespace PDCommon::Core {

PDContext::PDContext()
    : postProcessorManager_(std::make_unique<
          PDCommon::PostProcessing::PostProcessorManager>()) {}

PDContext::PDContext(const std::string &name)
    : name_(name), postProcessorManager_(std::make_unique<
                       PDCommon::PostProcessing::PostProcessorManager>()) {
  LOG_INFO(std::string("[PDContext] Start: ") + name_);
}

PDContext::~PDContext() = default;

void PDContext::createNeighborList(const PDCommon::Model::ParticleManager &mgr,
                                    double horizon) {
  neighborList_ = std::make_unique<PDCommon::Neighbor::NeighborList>();
  neighborList_->buildNeighbors(mgr, horizon);
}

} // namespace PDCommon::Core
