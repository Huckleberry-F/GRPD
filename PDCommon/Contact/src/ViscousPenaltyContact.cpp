#include "ViscousPenaltyContact.h"
#include "ContactRegistry.h"
#include "Logger.h"
#include "MechanicalMaterial.h"
#include "ParticleManager.h"
#include "PDContext.h"
#include <algorithm>
#include <cmath>

namespace PDCommon::Contact {

ViscousPenaltyContact::ViscousPenaltyContact(const std::string &name)
    : NodeNodeContact(name) {}

void ViscousPenaltyContact::initialize(const YAML::Node &configNode) {
  if (configNode["PenaltyFactor"])
    penaltyFactor_ = configNode["PenaltyFactor"].as<double>();
  if (configNode["PenaltyStiffness"])
    penaltyStiffness_ = configNode["PenaltyStiffness"].as<double>();
  if (configNode["DampingCoeff"])
    dampingCoeff_ = configNode["DampingCoeff"].as<double>();

  LOG_INFO("[ViscousPenaltyContact] Configured: Factor=" +
           std::to_string(penaltyFactor_) +
           ", DampingCoeff=" + std::to_string(dampingCoeff_));
}

void ViscousPenaltyContact::onPreContact(PDCommon::Core::PDContext &ctx,
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
    LOG_INFO("[ViscousPenaltyContact] Auto-Computed PenaltyStiffness: " +
             std::to_string(penaltyStiffness_));
  }
}

ContactPairResult ViscousPenaltyContact::computePairForce(
    const ContactPairContext &pair) {
  ContactPairResult result;

  // 弹簧力（位移相关）
  double f_spring = penaltyStiffness_ * pair.raw_penetration;

  // 粘性阻尼力（速度相关）
  double f_damp = 0.0;
  if (pair.vel) {
    double vxi = pair.vel[pair.i * 3];
    double vyi = pair.vel[pair.i * 3 + 1];
    double vzi = pair.vel[pair.i * 3 + 2];
    double vxj = pair.vel[pair.j * 3];
    double vyj = pair.vel[pair.j * 3 + 1];
    double vzj = pair.vel[pair.j * 3 + 2];

    double dvx = vxi - vxj, dvy = vyi - vyj, dvz = vzi - vzj;
    double v_rel_n = dvx * pair.nx + dvy * pair.ny + dvz * pair.nz;

    double m_eff =
        (pair.mass_i * pair.mass_j) / (pair.mass_i + pair.mass_j + 1e-30);
    double c_crit = 2.0 * std::sqrt(penaltyStiffness_ * m_eff);
    f_damp = dampingCoeff_ * c_crit * (-v_rel_n);

    // 阻尼力仅在粒子逼近时施加，防止阻碍分离
    if (v_rel_n > 0.0)
      f_damp = 0.0;
  }

  double forceMagnitude = f_spring + f_damp;
  // 总力不允许为负（不允许变成吸引力）
  if (forceMagnitude < 0.0)
    forceMagnitude = 0.0;

  result.fx = forceMagnitude * pair.nx;
  result.fy = forceMagnitude * pair.ny;
  result.fz = forceMagnitude * pair.nz;
  return result;
}

} // namespace PDCommon::Contact

REGISTER_CONTACT_TYPE(ViscousPenaltyContact, [](const std::string &name) {
  return std::make_unique<PDCommon::Contact::ViscousPenaltyContact>(name);
})
