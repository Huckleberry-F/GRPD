#include "PDEnginePre.h"
#include "ContactManager.h"
#include "ContactRegistry.h"
#include "FieldManager.h"
#include "ParticleManager.h"
#include "Logger.h"

namespace Src::Engine::Solvers::PD::Init {

void InitContact(PDCommon::Core::PDContext &ctx, const YAML::Node &config) {
    LOG_INFO("[InitContact] ==================================================");
    LOG_INFO("[InitContact] Entering Contact Initialization Phase...");
    LOG_INFO("[InitContact] ==================================================");

    // 首先在根目录搜索，若没有则为了向下兼容去 Solver 里找
    YAML::Node pairsNode;
    if (config["ContactPairs"]) {
        pairsNode = config["ContactPairs"];
    } else if (config["Solver"] && config["Solver"]["ContactPairs"]) {
        pairsNode = config["Solver"]["ContactPairs"];
    } else {
        LOG_INFO("[InitContact] No explicit ContactPairs defined in YAML. Contact engine skipping.");
        return;
    }

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
            // 通过 ContactRegistry 工厂动态创建（多态调度，无需硬编码类型分支）
            auto& registry = PDCommon::Contact::ContactRegistry::getInstance();
            auto contactAlg = registry.createContact(type, name);
            if (!contactAlg) {
                LOG_ERROR("[InitContact] Unknown Contact Type: '" + type +
                          "'. Registered types: " + [&]() {
                              std::string s;
                              for (const auto& t : registry.getRegisteredTypes())
                                  s += t + " ";
                              return s;
                          }());
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

            // 提取人工质量缩放比以修复显式接触动力学崩溃
            double massScale = 1.0;
            if (config["Solver"] && config["Solver"]["MassScaleFactor"]) {
                massScale = config["Solver"]["MassScaleFactor"].as<double>();
            }
            contactAlg->setMassScaleFactor(massScale);

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
