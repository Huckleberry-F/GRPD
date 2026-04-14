#include "SillingContact.h"
#include "ContactRegistry.h"
#include "Logger.h"
#include "StringUtils.h"
#include "MechanicalMaterial.h"
#include "PDContext.h"
#include "ParticleManager.h"
#include <algorithm>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace PDCommon::Contact {

SillingContact::SillingContact(const std::string &name)
    : NodeNodeContact(name) {}

void SillingContact::initialize(const YAML::Node &configNode) {
  if (configNode["ShortRangeStiffness"])
    shortRangeStiffness_ = configNode["ShortRangeStiffness"].as<double>();
  if (configNode["StiffnessFactor"])
    stiffnessFactor_ = configNode["StiffnessFactor"].as<double>();

  LOG_INFO("[SillingContact] Configured: StiffnessFactor=" +
           PDCommon::Utils::StringUtils::toScientific(stiffnessFactor_));
}

void SillingContact::onPreContact(PDCommon::Core::PDContext &ctx,
                                   double maxDx) {
  if (shortRangeStiffness_ < 0.0) {
    // 从 PD 微模量自动推导短程力密度常数 c_s
    auto &pm = ctx.getParticleManager();
    const auto &particles = pm.getAllParticles();
    const auto &neighborList = ctx.getNeighborList();
    double horizon = neighborList.getHorizon();

    // 获取体积模量（取主从面中较小的）
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
    double K = std::min(masterBulk, slaveBulk);

    dim_ = ctx.getDimension();
    effectiveThickness_ = (dim_ == 2) ? ctx.getThickness() : 1.0;

    if (dim_ == 2) {
      // 2D 键基微模量 (平面应力/应变): c = 9K / (π t δ³)
      // 短程力密度常数: c_s = c / δ = 9K / (π t δ⁴)
      // 这能确保最终与体积平方 (A1*t)*(A2*t) 相乘时，整体接触力刚好与 t 成正比
      double delta4 = horizon * horizon * horizon * horizon;
      shortRangeStiffness_ = stiffnessFactor_ * 9.0 * K / (M_PI * effectiveThickness_ * delta4);
    } else {
      // 3D 键基微模量: c = 18K / (π δ⁴)
      // 短程力密度常数: c_s = c / δ = 18K / (π δ⁵)
      double delta5 = horizon * horizon * horizon * horizon * horizon;
      shortRangeStiffness_ = stiffnessFactor_ * 18.0 * K / (M_PI * delta5);
    }

    LOG_INFO("[SillingContact] Auto: horizon=" + PDCommon::Utils::StringUtils::toScientific(horizon) +
             ", dx=" + std::to_string(maxDx) +
             ", K=" + PDCommon::Utils::StringUtils::toScientific(K) +
             ", c_s=" + std::to_string(shortRangeStiffness_));
  }
}

ContactPairResult
SillingContact::computePairForce(const ContactPairContext &pair) {
  ContactPairResult result;

  // Silling 短程力密度: f_s = c_s * penetration  [N/m⁶]
  double f_density = shortRangeStiffness_ * pair.raw_penetration;

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

REGISTER_CONTACT_TYPE(SillingContact, [](const std::string &name) {
  return std::make_unique<PDCommon::Contact::SillingContact>(name);
})
