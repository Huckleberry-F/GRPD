#include "PDEnginePre.h"
#include "ContactManager.h"
#include "ContactRegistry.h"
#include "PenaltyContact.h"
#include "FieldManager.h"
#include "ParticleManager.h"
#include "Logger.h"

namespace Src::Engine::Solvers::PD::Init {

void InitContact(PDCommon::Core::PDContext &ctx, const YAML::Node &config) {
    LOG_INFO("[InitContact] ==================================================");
    LOG_INFO("[InitContact] Entering Contact Initialization Phase...");
    LOG_INFO("[InitContact] ==================================================");

    if (!config["Solver"]["ContactPairs"]) {
        LOG_INFO("[InitContact] No explicit ContactPairs defined in YAML. Contact engine skipping.");
        return;
    }

    auto pairsNode = config["Solver"]["ContactPairs"];
    if (!pairsNode.IsSequence()) {
        LOG_ERROR("[InitContact] ContactPairs must be a sequence/array in YAML.");
        return;
    }

    auto& pm = ctx.getParticleManager();
    const auto& particles = pm.getAllParticles();

    for (size_t i = 0; i < pairsNode.size(); ++i) {
        auto node = pairsNode[i];
        std::string name = node["Name"] ? node["Name"].as<std::string>() : "ContactPair_" + std::to_string(i);
        std::string type = node["Type"] ? node["Type"].as<std::string>() : "PenaltyContact";
        
        int masterPart = node["MasterPartID"] ? node["MasterPartID"].as<int>() : -1;
        int slavePart = node["SlavePartID"] ? node["SlavePartID"].as<int>() : -1;

        if (masterPart == -1 || slavePart == -1) {
            LOG_WARNING("[InitContact] ContactPair '" + name + "' missing MasterPartID or SlavePartID. Skipping.");
            continue;
        }

        try {
            std::unique_ptr<PDCommon::Contact::IContactAlgorithm> contactAlg;
            if (type == "PenaltyContact") {
                contactAlg = std::make_unique<PDCommon::Contact::PenaltyContact>(name);
            } else {
                LOG_ERROR("[InitContact] Unknown Contact Type: " + type);
                continue;
            }

            contactAlg->initialize(node);

            std::vector<int> masterIds;
            std::vector<int> slaveIds;

            for (size_t p = 0; p < particles.size(); ++p) {
                // 我们不在此处筛选 isSurface，而是把整个 Part 都放入，这样能兼容未来的断裂动态表面暴露
                if (particles[p].getPartId() == masterPart) {
                    masterIds.push_back(p);
                }
                if (particles[p].getPartId() == slavePart) {
                    slaveIds.push_back(p);
                }
            }

            contactAlg->setMasterParticleIds(masterIds);
            contactAlg->setSlaveParticleIds(slaveIds);

            ctx.getContactManager().addContactPair(std::move(contactAlg));
            LOG_INFO("[InitContact] Defined ContactPair: " + name + " [Master Part " + 
                     std::to_string(masterPart) + " (" + std::to_string(masterIds.size()) + " nodes) <- Slave Part " + 
                     std::to_string(slavePart) + " (" + std::to_string(slaveIds.size()) + " nodes)]");

        } catch (const std::exception& e) {
            LOG_ERROR(std::string("[InitContact] Failed to create Contact algorithm: ") + e.what());
        }
    }
}

} // namespace Src::Engine::Solvers::PD::Init
