#include "PostProcessorManager.h"
#include <iostream>

namespace PDCommon::PostProcessing {

void PostProcessorManager::addPostProcessor(std::unique_ptr<PostProcessorBase> pp) {
  if (pp) {
    processors_.push_back(std::move(pp));
  }
}

void PostProcessorManager::initializeAll(PDCommon::Core::PDContext &ctx,
                                         const YAML::Node &configNode) {
  // Config node is passed to each processor during their specific creation,
  // but if we need a global pass, we can do it here. 
  // Normally initialize is called right after creation in PDEngine.
}

void PostProcessorManager::executePreBCHooks(PDCommon::Core::PDContext &ctx,
                                             double currentTime, int stepId) {
  for (auto &pp : processors_) {
    pp->preBCEvaluate(ctx, currentTime, stepId);
  }
}

void PostProcessorManager::executePostStepHooks(PDCommon::Core::PDContext &ctx,
                                                double currentTime,
                                                int stepId) {
  for (auto &pp : processors_) {
    pp->postStepEvaluate(ctx, currentTime, stepId);
  }
}

} // namespace PDCommon::PostProcessing
