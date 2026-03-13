// ============================================================================
// PDSimulater.cpp - 近场动力学仿真总控类实现
// ============================================================================

#include "PDSimulater.h"
#include "Logger.h"
#include <string>

namespace GRPD::Core {

PDSimulater::PDSimulater(const std::string &name) : name_(name) {
  LOG_INFO(std::string("[PDSimulater] Start: ") + name_);
}

void PDSimulater::createNeighborList(const GRPD::Model::ParticleManager &mgr,
                                    double horizon) {
  neighborList_ = std::make_unique<GRPD::Initial::NeighborList>();
  neighborList_->buildNeighbors(mgr, horizon);
}

} // namespace GRPD::Core
