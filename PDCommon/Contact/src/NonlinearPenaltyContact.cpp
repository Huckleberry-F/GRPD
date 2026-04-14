#include "NonlinearPenaltyContact.h"
#include "ContactRegistry.h"
#include "Logger.h"
#include "MechanicalMaterial.h"
#include "ParticleManager.h"
#include "PDContext.h"
#include <algorithm>
#include <cmath>

namespace PDCommon::Contact {

NonlinearPenaltyContact::NonlinearPenaltyContact(const std::string &name)
    : NodeNodeContact(name) {}

void NonlinearPenaltyContact::initialize(const YAML::Node &configNode) {
  if (configNode["PenaltyFactor"])
    penaltyFactor_ = configNode["PenaltyFactor"].as<double>();
  if (configNode["PenaltyStiffness"])
    penaltyStiffness_ = configNode["PenaltyStiffness"].as<double>();
  if (configNode["NonlinearOnsetRatio"])
    nonlinearOnsetRatio_ = configNode["NonlinearOnsetRatio"].as<double>();
  if (configNode["StiffeningCoeff"])
    stiffeningCoeff_ = configNode["StiffeningCoeff"].as<double>();

  LOG_INFO("[NonlinearPenaltyContact] Configured: Factor=" +
           std::to_string(penaltyFactor_) +
           ", OnsetRatio=" + std::to_string(nonlinearOnsetRatio_) +
           ", StiffeningCoeff=" + std::to_string(stiffeningCoeff_));
}

void NonlinearPenaltyContact::onPreContact(PDCommon::Core::PDContext &ctx,
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
    const int dim = ctx.getDimension();
    double effectiveThickness = (dim == 2) ? ctx.getThickness() : 1.0;
    penaltyStiffness_ = penaltyFactor_ * minK * maxDx * effectiveThickness;
    LOG_INFO("[NonlinearPenaltyContact] Auto-Computed PenaltyStiffness: " +
             std::to_string(penaltyStiffness_));
  }
}

ContactPairResult NonlinearPenaltyContact::computePairForce(
    const ContactPairContext &pair) {
  ContactPairResult result;

  // 渐进非线性强化系数
  double penetration_ratio = pair.raw_penetration / pair.safeDist;
  double stiffening_factor = 1.0;
  if (penetration_ratio > nonlinearOnsetRatio_) {
    double excess = penetration_ratio - nonlinearOnsetRatio_;
    stiffening_factor = 1.0 + stiffeningCoeff_ * (excess * excess);
  }

  double forceMagnitude =
      penaltyStiffness_ * stiffening_factor * pair.raw_penetration;

  result.fx = forceMagnitude * pair.nx;
  result.fy = forceMagnitude * pair.ny;
  result.fz = forceMagnitude * pair.nz;
  return result;
}

} // namespace PDCommon::Contact

REGISTER_CONTACT_TYPE(NonlinearPenaltyContact, [](const std::string &name) {
  return std::make_unique<PDCommon::Contact::NonlinearPenaltyContact>(name);
})
