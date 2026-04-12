#include "PenaltyContact.h"
#include "ContactRegistry.h"
#include "Logger.h"
#include "MechanicalMaterial.h"
#include "ParticleManager.h"
#include "PDContext.h"
#include <algorithm>
#include <cmath>

namespace PDCommon::Contact {

PenaltyContact::PenaltyContact(const std::string &name)
    : NodeNodeContact(name) {}

void PenaltyContact::initialize(const YAML::Node &configNode) {
  if (configNode["PenaltyFactor"]) {
    penaltyFactor_ = configNode["PenaltyFactor"].as<double>();
  }
  if (configNode["PenaltyStiffness"]) {
    penaltyStiffness_ = configNode["PenaltyStiffness"].as<double>();
  }
  if (configNode["PinballRatio"]) {
    pinballRatio_ = configNode["PinballRatio"].as<double>();
  }
  LOG_INFO("[PenaltyContact] Configured: Factor = " +
           std::to_string(penaltyFactor_) +
           ", Pinball = " + std::to_string(pinballRatio_));
}

void PenaltyContact::onPreContact(PDCommon::Core::PDContext &ctx,
                                   double maxDx) {
  if (penaltyStiffness_ < 0.0) {
    auto &pm = ctx.getParticleManager();
    const auto &particles = pm.getAllParticles();
    double masterBulk = 1.0e5, slaveBulk = 1.0e5;
    if (!masterIds_.empty()) {
      auto *matBase = particles[masterIds_[0]].getMaterial();
      auto *mechMat =
          dynamic_cast<PDCommon::Material::MechanicalMaterial *>(matBase);
      if (mechMat)
        masterBulk = mechMat->getBulkModulus();
    }
    if (!slaveIds_.empty()) {
      auto *matBase = particles[slaveIds_[0]].getMaterial();
      auto *mechMat =
          dynamic_cast<PDCommon::Material::MechanicalMaterial *>(matBase);
      if (mechMat)
        slaveBulk = mechMat->getBulkModulus();
    }
    double minK = std::min(masterBulk, slaveBulk);
    penaltyStiffness_ = penaltyFactor_ * minK * maxDx;
    LOG_INFO("[PenaltyContact] Auto-Computed PenaltyStiffness: " +
             std::to_string(penaltyStiffness_) +
             " (Factor: " + std::to_string(penaltyFactor_) + ")");
  }
}

ContactPairResult
PenaltyContact::computePairForce(const ContactPairContext &pair) {
  ContactPairResult result;

  double effective_penetration =
      std::min(pair.raw_penetration, pinballRatio_ * pair.safeDist);
  double forceMagnitude = penaltyStiffness_ * effective_penetration;

  result.fx = forceMagnitude * pair.nx;
  result.fy = forceMagnitude * pair.ny;
  result.fz = forceMagnitude * pair.nz;
  return result;
}

} // namespace PDCommon::Contact

REGISTER_CONTACT_TYPE(PenaltyContact, [](const std::string &name) {
  return std::make_unique<PDCommon::Contact::PenaltyContact>(name);
})
