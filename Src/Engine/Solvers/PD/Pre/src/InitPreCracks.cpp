#include "PDEnginePre.h"
#include "Logger.h"
#include "PreCrackRegistry.h"
#include <yaml-cpp/yaml.h>

namespace Src::Engine::Solvers::PD::Init {

void InitPreCracks(PDCommon::Core::PDContext &ctx, const YAML::Node &config) {
    if (config["PreCracks"]) {
        LOG_INFO("[InitPreCracks] Detected PreCracks configuration. Initializing geometric cuts...");
        const YAML::Node& cracks = config["PreCracks"];
        if (cracks.IsSequence()) {
            for (auto crackNode : cracks) {
                if (crackNode["Type"]) {
                    std::string type = crackNode["Type"].as<std::string>();
                    if (PDCommon::Fracture::PreCrackRegistry::getInstance().contains(type)) {
                        auto crackModel = PDCommon::Fracture::PreCrackRegistry::getInstance().create(type);
                        crackModel->configure(crackNode);
                        crackModel->apply(ctx);
                        LOG_INFO("[InitPreCracks] Applied PreCrack of type: " + type);
                    } else {
                        LOG_ERROR("[InitPreCracks] PreCrack Model '" + type + "' not found in registry.");
                    }
                }
            }
        }
    }
}

} // namespace Src::Engine::Solvers::PD::Init
