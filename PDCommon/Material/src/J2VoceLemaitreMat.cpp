#include "J2VoceLemaitreMat.h"
#include "FieldManager.h"
#include "FieldRegistry.h"
#include "Logger.h"
#include "MaterialRegistry.h"
#include <algorithm>
#include <cmath>

namespace PDCommon::Material {

REGISTER_MATERIAL_TYPE(J2VoceLemaitre, [](const std::string &name) {
  return std::make_unique<PDCommon::Material::J2VoceLemaitreMat>(name);
});

J2VoceLemaitreMat::J2VoceLemaitreMat(const std::string &name)
    : MechanicalMaterial(name) {}

void J2VoceLemaitreMat::initialize(const YAML::Node &matNode) {
  MechanicalMaterial::initialize(matNode);

  if (matNode["Density"])
    density_ = matNode["Density"].as<double>();
  if (matNode["YoungsModulus"])
    youngsModulus_ = matNode["YoungsModulus"].as<double>();
  if (matNode["PoissonsRatio"])
    poissonsRatio_ = matNode["PoissonsRatio"].as<double>();

  if (matNode["YieldStress"])
    yieldStress_ = matNode["YieldStress"].as<double>();
  if (matNode["LinearHardening"])
    linearHardening_ = matNode["LinearHardening"].as<double>();
  if (matNode["VoceR"])
    voceR_ = matNode["VoceR"].as<double>();
  if (matNode["VoceB"])
    voceB_ = matNode["VoceB"].as<double>();

  if (matNode["LemaitreS"])
    lemaitreS_ = matNode["LemaitreS"].as<double>();
  if (matNode["Lemaitre_s"])
    lemaitre_s_ = matNode["Lemaitre_s"].as<double>();
  if (matNode["DamageThreshold"])
    damageThreshold_ = matNode["DamageThreshold"].as<double>();
  if (matNode["CriticalDamage"])
    criticalDamage_ = matNode["CriticalDamage"].as<double>();
  if (matNode["DamageAccelThreshold"])
    damageAccelThreshold_ = matNode["DamageAccelThreshold"].as<double>();
  if (matNode["DamageAccelFactor"])
    damageAccelFactor_ = matNode["DamageAccelFactor"].as<double>();
  if (matNode["LargeDeformation"])
    largeDeformation_ = matNode["LargeDeformation"].as<bool>();

  if (!std::isfinite(density_) || !std::isfinite(youngsModulus_) ||
      !std::isfinite(poissonsRatio_) || density_ <= 0.0 ||
      youngsModulus_ <= 0.0 || poissonsRatio_ < 0.0 || poissonsRatio_ >= 0.5) {
    LOG_ERROR(
        "[J2VoceLemaitreMat] Invalid basic elastic parameters for material: " +
        name_);
    exit(EXIT_FAILURE);
  }
  if (!std::isfinite(yieldStress_) || yieldStress_ <= 0.0) {
    LOG_ERROR("[J2VoceLemaitreMat] YieldStress must be positive: " + name_);
    exit(EXIT_FAILURE);
  }
  if (!std::isfinite(linearHardening_) || linearHardening_ < 0.0 ||
      !std::isfinite(voceR_) || voceR_ < 0.0 || !std::isfinite(voceB_) ||
      voceB_ < 0.0) {
    LOG_ERROR("[J2VoceLemaitreMat] Hardening parameters must be finite and "
              "non-negative: " +
              name_);
    exit(EXIT_FAILURE);
  }
  if (!std::isfinite(lemaitreS_) || lemaitreS_ <= 0.0) {
    LOG_ERROR("[J2VoceLemaitreMat] LemaitreS must be strictly positive: " +
              name_);
    exit(EXIT_FAILURE);
  }
  if (!std::isfinite(lemaitre_s_) || lemaitre_s_ <= 0.0) {
    LOG_ERROR("[J2VoceLemaitreMat] Lemaitre_s must be strictly positive: " +
              name_);
    exit(EXIT_FAILURE);
  }
  if (!std::isfinite(criticalDamage_) || criticalDamage_ <= 0.0 ||
      criticalDamage_ > 1.0) {
    LOG_ERROR("[J2VoceLemaitreMat] CriticalDamage must be in (0, 1]: " + name_);
    exit(EXIT_FAILURE);
  }

  computeLameParameters();
}

void J2VoceLemaitreMat::computeLameParameters() {
  lambda_ = (youngsModulus_ * poissonsRatio_) /
            ((1.0 + poissonsRatio_) * (1.0 - 2.0 * poissonsRatio_));
  mu_ = youngsModulus_ / (2.0 * (1.0 + poissonsRatio_));
  bulkModulus_ = youngsModulus_ / (3.0 * (1.0 - 2.0 * poissonsRatio_));
}

void J2VoceLemaitreMat::allocateStateVariables(
    PDCommon::Field::FieldManager &fm) {
  auto &reg = PDCommon::Field::FieldRegistry::getInstance();

  fm.addField(reg.createField("DoubleField", "EqPlasticStrain_Old", 1));
  fm.addField(reg.createField("DoubleField", "EqPlasticStrain_Trial", 1));
  fm.registerSwapPair("EqPlasticStrain_Old", "EqPlasticStrain_Trial");

  fm.addField(reg.createField("DoubleField", "RealEqPlasticStrain_Old", 1));
  fm.addField(reg.createField("DoubleField", "RealEqPlasticStrain_Trial", 1));
  fm.registerSwapPair("RealEqPlasticStrain_Old", "RealEqPlasticStrain_Trial");

  fm.addField(reg.createField("DoubleField", "PlasticStrain_Old", 9));
  fm.addField(reg.createField("DoubleField", "PlasticStrain_Trial", 9));
  fm.registerSwapPair("PlasticStrain_Old", "PlasticStrain_Trial");

  fm.addField(reg.createField("DoubleField", "Damage_Old", 1));
  fm.addField(reg.createField("DoubleField", "Damage_Trial", 1));
  fm.registerSwapPair("Damage_Old", "Damage_Trial");

  fm.addField(reg.createField("DoubleField", "VonMisesStress", 1));
}

void J2VoceLemaitreMat::bindStateVariables(PDCommon::Field::FieldManager &fm) {
  numParticles_ = fm.getFieldAs<double>("EqPlasticStrain_Old")->size();

  fieldEqPSOld_ = fm.getFieldAs<double>("EqPlasticStrain_Old");
  fieldEqPSTrial_ = fm.getFieldAs<double>("EqPlasticStrain_Trial");
  fieldRealEqPSOld_ = fm.getFieldAs<double>("RealEqPlasticStrain_Old");
  fieldRealEqPSTrial_ = fm.getFieldAs<double>("RealEqPlasticStrain_Trial");
  fieldPSOld_ = fm.getFieldAs<double>("PlasticStrain_Old");
  fieldPSTrial_ = fm.getFieldAs<double>("PlasticStrain_Trial");
  fieldDamageOld_ = fm.getFieldAs<double>("Damage_Old");
  fieldDamageTrial_ = fm.getFieldAs<double>("Damage_Trial");
  vonMises_ = fm.getFieldAs<double>("VonMisesStress")->dataPtr();

  eqPSOld_ = fieldEqPSOld_->dataPtr();
  eqPSTrial_ = fieldEqPSTrial_->dataPtr();
  realEqPSOld_ = fieldRealEqPSOld_->dataPtr();
  realEqPSTrial_ = fieldRealEqPSTrial_->dataPtr();
  pSOld_ = fieldPSOld_->dataPtr();
  pSTrial_ = fieldPSTrial_->dataPtr();
  damageOld_ = fieldDamageOld_->dataPtr();
  damageTrial_ = fieldDamageTrial_->dataPtr();
}

size_t J2VoceLemaitreMat::getNumStateVariables() const { return 0; }

void J2VoceLemaitreMat::evaluate() {}

void J2VoceLemaitreMat::commitState() {
  if (!fieldEqPSOld_ || !fieldEqPSTrial_ || !fieldRealEqPSOld_ ||
      !fieldRealEqPSTrial_ || !fieldPSOld_ || !fieldPSTrial_)
    return;

  eqPSOld_ = fieldEqPSOld_->dataPtr();
  eqPSTrial_ = fieldEqPSTrial_->dataPtr();
  realEqPSOld_ = fieldRealEqPSOld_->dataPtr();
  realEqPSTrial_ = fieldRealEqPSTrial_->dataPtr();
  pSOld_ = fieldPSOld_->dataPtr();
  pSTrial_ = fieldPSTrial_->dataPtr();

  if (fieldDamageOld_ && fieldDamageTrial_) {
    damageOld_ = fieldDamageOld_->dataPtr();
    damageTrial_ = fieldDamageTrial_->dataPtr();
  }
}

Eigen::Matrix3d J2VoceLemaitreMat::ComputeEngineeringStress(
    const Eigen::Matrix3d &strain) const {
  return 2.0 * mu_ * strain +
         lambda_ * strain.trace() * Eigen::Matrix3d::Identity();
}

Eigen::Matrix3d J2VoceLemaitreMat::ComputePK1Stress(const Eigen::Matrix3d &F,
                                                    int particleId,
                                                    int stateMode) const {
  if (!F.allFinite()) {
    return Eigen::Matrix3d::Zero();
  }

  if (particleId < 0 || eqPSOld_ == nullptr || eqPSTrial_ == nullptr ||
      realEqPSOld_ == nullptr || realEqPSTrial_ == nullptr ||
      pSOld_ == nullptr || pSTrial_ == nullptr || damageOld_ == nullptr ||
      damageTrial_ == nullptr || vonMises_ == nullptr) {
    Eigen::Matrix3d strain =
        0.5 * (F + F.transpose()) - Eigen::Matrix3d::Identity();
    return ComputeEngineeringStress(strain);
  }

  const Eigen::Matrix3d I = Eigen::Matrix3d::Identity();
  constexpr double minW = 0.001;
  constexpr double tol = 1.0e-8;
  constexpr int maxIter = 200;
  constexpr double tiny = 1.0e-30;

  Eigen::Matrix3d eps = Eigen::Matrix3d::Zero();
  if (largeDeformation_) {
    eps = 0.5 * (F.transpose() * F) - I;
  } else {
    eps = 0.5 * (F + F.transpose()) - I;
  }

  const int idx9 = particleId * 9;

  auto readMatrix = [](const double *data, int offset) {
    Eigen::Matrix3d value = Eigen::Matrix3d::Zero();
    value << data[offset], data[offset + 1], data[offset + 2],
        data[offset + 3], data[offset + 4], data[offset + 5],
        data[offset + 6], data[offset + 7], data[offset + 8];
    if (!value.allFinite()) {
      value.setZero();
    }
    return value;
  };

  auto writeMatrix = [](double *data, int offset, const Eigen::Matrix3d &value) {
    data[offset] = value(0, 0);
    data[offset + 1] = value(0, 1);
    data[offset + 2] = value(0, 2);
    data[offset + 3] = value(1, 0);
    data[offset + 4] = value(1, 1);
    data[offset + 5] = value(1, 2);
    data[offset + 6] = value(2, 0);
    data[offset + 7] = value(2, 1);
    data[offset + 8] = value(2, 2);
  };

  auto returnStress = [&](const Eigen::Matrix3d &sigma) -> Eigen::Matrix3d {
    if (!sigma.allFinite()) {
      return Eigen::Matrix3d::Zero();
    }
    if (largeDeformation_) {
      Eigen::Matrix3d P = F * sigma;
      return P.allFinite() ? P : Eigen::Matrix3d::Zero().eval();
    }
    return sigma;
  };

  auto degradedStress = [&](const Eigen::Matrix3d &sDev, double sM,
                            double W) {
    const double p = (sM >= 0.0) ? W * sM : sM;
    return W * sDev + p * I;
  };

  auto degradedElasticStress = [&](const Eigen::Matrix3d &epsE, double W) {
    Eigen::Matrix3d sTrial = ComputeEngineeringStress(epsE);
    const double sM = sTrial.trace() / 3.0;
    Eigen::Matrix3d sDev = sTrial - sM * I;
    return degradedStress(sDev, sM, W);
  };

  auto yieldStressAt = [this](double alpha) {
    return yieldStress_ + linearHardening_ * alpha +
           voceR_ * (1.0 - std::exp(-voceB_ * alpha));
  };

  auto hardeningAt = [this](double alpha) {
    return linearHardening_ + voceR_ * voceB_ * std::exp(-voceB_ * alpha);
  };

  if (stateMode == 1 || stateMode == 2) {
    const double *psPtr = (stateMode == 1) ? pSOld_ : pSTrial_;
    const double damage =
        (stateMode == 1) ? damageOld_[particleId] : damageTrial_[particleId];
    const double W = std::max(1.0 - (std::isfinite(damage) ? damage : 0.0), minW);
    return returnStress(degradedElasticStress(eps - readMatrix(psPtr, idx9), W));
  }

  Eigen::Matrix3d epsPOld = readMatrix(pSOld_, idx9);
  double alphaOld =
      std::isfinite(eqPSOld_[particleId]) ? eqPSOld_[particleId] : 0.0;
  double realEqOld =
      std::isfinite(realEqPSOld_[particleId]) ? realEqPSOld_[particleId] : 0.0;
  double damageOld =
      std::isfinite(damageOld_[particleId]) ? damageOld_[particleId] : 0.0;
  damageOld = std::max(0.0, damageOld);
  double Wn = std::max(1.0 - damageOld, minW);

  if (damageOld >= criticalDamage_) {
    eqPSTrial_[particleId] = alphaOld;
    realEqPSTrial_[particleId] = realEqOld;
    writeMatrix(pSTrial_, idx9, epsPOld);
    damageTrial_[particleId] = damageOld;
    Eigen::Matrix3d sigma = degradedElasticStress(eps - epsPOld, Wn);
    Eigen::Matrix3d dev = sigma - sigma.trace() / 3.0 * I;
    vonMises_[particleId] =
        dev.allFinite() ? std::sqrt(1.5 * dev.cwiseProduct(dev).sum()) : 0.0;
    return returnStress(sigma);
  }

  Eigen::Matrix3d epsE = eps - epsPOld;
  Eigen::Matrix3d sTrial = ComputeEngineeringStress(epsE);
  const double sM = sTrial.trace() / 3.0;
  Eigen::Matrix3d sDevTrial = sTrial - sM * I;
  const double J2 = std::sqrt(1.5 * sDevTrial.cwiseProduct(sDevTrial).sum());

  double gamma = 0.0;
  double alphaNew = alphaOld;
  double realEqNew = realEqOld;
  double W = Wn;
  double SY = yieldStressAt(alphaOld);
  Eigen::Matrix3d sigma = degradedStress(sDevTrial, sM, Wn);
  Eigen::Matrix3d epsPNew = epsPOld;
  double mises = Wn * (std::isfinite(J2) ? J2 : 0.0);

  if (std::isfinite(J2) && J2 > 1.0e-16 && std::isfinite(SY)) {
    const double f = J2 - SY;
    if (f > tol) {
      gamma = Wn * f / (3.0 * mu_);
      alphaNew = alphaOld + gamma;
      realEqNew = realEqOld + gamma / Wn;

      double res1 = 1.0;
      int iter = 1;
      while (std::abs(res1) > tol && iter <= maxIter) {
        SY = yieldStressAt(alphaNew);
        res1 = J2 - 3.0 * mu_ * gamma / Wn - SY;
        const double H11 = -3.0 * mu_ / Wn - hardeningAt(alphaNew);
        if (std::abs(H11) < tiny) {
          break;
        }

        gamma += -res1 / H11;
        alphaNew = alphaOld + gamma;
        realEqNew = realEqOld + gamma / Wn;
        ++iter;
      }

      SY = yieldStressAt(alphaNew);
      const double q = Wn * SY;
      const double p = (sM >= 0.0) ? Wn * sM : sM;
      Eigen::Matrix3d sDevNew = (q / J2) * sDevTrial;
      sigma = sDevNew + p * I;

      Eigen::Matrix3d epsENew =
          sDevNew / (2.0 * mu_ * Wn) +
          ((sM >= 0.0) ? p / (3.0 * Wn * bulkModulus_)
                       : p / (3.0 * bulkModulus_)) *
              I;
      epsPNew = epsPOld + epsE - epsENew;
      mises = q;

      if (realEqNew > damageThreshold_) {
        double Y = -(q * q / (6.0 * mu_ * Wn * Wn));
        if (sM >= 0.0) {
          Y -= sM * sM / (2.0 * bulkModulus_);
        }
        if (damageOld >= damageAccelThreshold_) {
          Y *= damageAccelFactor_;
        }

        const double f2 = -Y / lemaitreS_;
        const double f2s = std::pow(f2, lemaitre_s_);
        double res2 = 1.0;
        iter = 1;
        while (std::abs(res2) > tol && iter <= maxIter) {
          res2 = W - Wn + f2s * gamma / W;
          const double H22 = 1.0 - f2s * gamma / (W * W);
          if (std::abs(H22) < tiny) {
            break;
          }
          W += -res2 / H22;
          ++iter;
        }

        if (W < minW) {
          W = minW;
        }
      }
    }
  }

  eqPSTrial_[particleId] = alphaNew;
  realEqPSTrial_[particleId] = realEqNew;
  writeMatrix(pSTrial_, idx9, epsPNew);
  damageTrial_[particleId] = std::max(damageOld, 1.0 - W);
  vonMises_[particleId] = mises;

  return returnStress(sigma);
}

} // namespace PDCommon::Material
