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

    // Silling & Askari (2005) 微模量 (3D bond-based):
    //   c = 18K / (π δ⁴)            [N/m⁶] (force density 单位)
    // 短程力密度常数:
    //   c_s = c / δ = 18K / (π δ⁵)  [N/m⁷]
    // 使得 f_s = c_s * penetration   [N/m⁶] (与 PD 内力同量纲)
    double delta5 = horizon * horizon * horizon * horizon * horizon;
    shortRangeStiffness_ = stiffnessFactor_ * 18.0 * K / (M_PI * delta5);

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

  // 关键量纲转换：PD 力密度 → 实际力
  // 在 PD 框架中: ρ ü_i = Σ_j f(ξ) V_j
  //   → ü_i = f(ξ) V_j / ρ
  // 在我们框架中: ü_i = F / mass_i = F / (ρ V_i)
  //   → 要求 F = f_density * V_i * V_j  [N]
  double vol_i = pair.dx_i * pair.dx_i * pair.dx_i;
  double vol_j = pair.dx_j * pair.dx_j * pair.dx_j;
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
