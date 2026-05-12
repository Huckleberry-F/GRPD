#include "PDEnginePre.h"
#include "PostProcessorRegistry.h"
#include "PostProcessorManager.h"
#include "Logger.h"

namespace Src::Engine::Solvers::PD::Init {

void InitPostProcessing(PDCommon::Core::PDContext &ctx, const YAML::Node &config) {
  if (!config["PostProcessing"] || !config["PostProcessing"].IsSequence()) {
    return;
  }

  LOG_INFO("[PDEngine::Init] Initializing PostProcessing modules...");

  auto &registry = PDCommon::PostProcessing::PostProcessorRegistry::getInstance();
  auto &manager = ctx.getPostProcessorManager();

  for (size_t i = 0; i < config["PostProcessing"].size(); ++i) {
    auto ppNode = config["PostProcessing"][i];
    if (!ppNode["Type"]) {
      LOG_WARNING("[PDEngine::Init] PostProcessing node missing 'Type'. Skipping.");
      continue;
    }

    std::string type = ppNode["Type"].as<std::string>();
    auto pp = registry.create(type);

    if (pp) {
      pp->initialize(ctx, ppNode);
      manager.addPostProcessor(std::move(pp));
      LOG_INFO("[PDEngine::Init] Registered PostProcessor: " + type);
    } else {
      LOG_WARNING("[PDEngine::Init] Failed to create PostProcessor of type: " + type);
    }
  }
}

} // namespace Src::Engine::Solvers::PD::Init
