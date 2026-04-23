#include "SillingForceLaw.h"
#include "ContactRegistry.h"
#include "Logger.h"
#include "MechanicalMaterial.h"
#include "PDContext.h"
#include "ParticleManager.h"
#include "StringUtils.h"
#include <algorithm>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace PDCommon::Contact {

void SillingForceLaw::initialize(const YAML::Node &configNode) {
  if (configNode["ShortRangeStiffness"])
    shortRangeStiffness_ = configNode["ShortRangeStiffness"].as<double>();
  if (configNode["StiffnessFactor"])
    stiffnessFactor_ = configNode["StiffnessFactor"].as<double>();

  LOG_INFO("[SillingForceLaw] Configured: StiffnessFactor=" +
           PDCommon::Utils::StringUtils::toScientific(stiffnessFactor_));
}

void SillingForceLaw::onPreContact(PDCommon::Core::PDContext &ctx,
                                    double maxDx) {
  if (shortRangeStiffness_ < 0.0) {
    auto &pm = ctx.getParticleManager();
    const auto &particles = pm.getAllParticles();
    const auto &neighborList = ctx.getNeighborList();
    double horizon = neighborList.getHorizon();

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
    double K = std::max(masterBulk, slaveBulk);

    dim_ = ctx.getDimension();
    effectiveThickness_ = (dim_ == 2) ? ctx.getThickness() : 1.0;

    if (dim_ == 2) {
      double delta3 = horizon * horizon * horizon;
      shortRangeStiffness_ =
          stiffnessFactor_ * 9.0 * K / (M_PI * effectiveThickness_ * delta3);
    } else {
      double delta4 = horizon * horizon * horizon * horizon;
      shortRangeStiffness_ = stiffnessFactor_ * 18.0 * K / (M_PI * delta4);
    }

    LOG_INFO("[SillingForceLaw] Auto: horizon=" +
             PDCommon::Utils::StringUtils::toScientific(horizon) +
             ", dx=" + std::to_string(maxDx) +
             ", K=" + PDCommon::Utils::StringUtils::toScientific(K) +
             ", c_s=" + std::to_string(shortRangeStiffness_));
  }
}

ForceResult SillingForceLaw::computeForce(const ContactContext &pair) {
  ForceResult result;

  double s_contact = pair.raw_penetration / pair.safeDist;
  double f_density = shortRangeStiffness_ * s_contact;

  double vol_i = (dim_ == 2) ? (pair.dx_i * pair.dx_i * effectiveThickness_)
                             : (pair.dx_i * pair.dx_i * pair.dx_i);
  double vol_j = (dim_ == 2) ? (pair.dx_j * pair.dx_j * effectiveThickness_)
                             : (pair.dx_j * pair.dx_j * pair.dx_j);
  double forceMagnitude = f_density * vol_i * vol_j;

  result.fx = forceMagnitude * pair.nx;
  result.fy = forceMagnitude * pair.ny;
  result.fz = forceMagnitude * pair.nz;
  return result;
}

} // namespace PDCommon::Contact

REGISTER_FORCELAW_TYPE(Silling, SillingForceLaw)

