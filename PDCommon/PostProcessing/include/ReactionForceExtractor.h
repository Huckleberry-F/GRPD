#ifndef PDCOMMON_POSTPROCESSING_REACTIONFORCEEXTRACTOR_H
#define PDCOMMON_POSTPROCESSING_REACTIONFORCEEXTRACTOR_H

#include "PostProcessorBase.h"
#include <string>

namespace PDCommon::PostProcessing {

class ReactionForceExtractor : public PostProcessorBase {
public:
  ReactionForceExtractor() = default;
  ~ReactionForceExtractor() override = default;

  void initialize(PDCommon::Core::PDContext &ctx,
                  const YAML::Node &config) override;

  void preBCEvaluate(PDCommon::Core::PDContext &ctx, double currentTime,
                     int stepId) override;

private:
  std::string name_ = "ReactionForceExtractor";
};

} // namespace PDCommon::PostProcessing

#endif // PDCOMMON_POSTPROCESSING_REACTIONFORCEEXTRACTOR_H
