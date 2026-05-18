#include "J2VoceLemaitreMat.h"
#include "FieldManager.h"
#include "FieldRegistry.h"
#include "Logger.h"
#include "MaterialRegistry.h"
#include <algorithm>
#include <cmath>

namespace PDCommon::Material {

namespace {

constexpr double kExpArgLimit = 700.0;
constexpr double kMaxEqPlasticStrain = 1.0e6;

double ClampFinite(double value, double lower, double upper, double fallback) {
  if (!std::isfinite(value)) {
    return fallback;
  }
  if (upper < lower) {
    return lower;
  }
  return std::clamp(value, lower, upper);
}

double SafeExp(double value) {
  if (!std::isfinite(value)) {
    return value > 0.0 ? std::exp(kExpArgLimit) : 0.0;
  }
  return std::exp(std::clamp(value, -kExpArgLimit, kExpArgLimit));
}

bool SafePow(double base, double exponent, double &result) {
  if (!std::isfinite(base) || !std::isfinite(exponent) || base < 0.0) {
    return false;
  }
  if (base == 0.0) {
    if (exponent < 0.0) {
      return false;
    }
    result = (exponent == 0.0) ? 1.0 : 0.0;
    return true;
  }

  const double logValue = exponent * std::log(base);
  if (!std::isfinite(logValue) || logValue > kExpArgLimit) {
    return false;
  }
  result = (logValue < -kExpArgLimit) ? 0.0 : std::exp(logValue);
  return std::isfinite(result);
}

} // namespace

// ---------------------------------------------------------------------------
// 向工厂注册材料类型
// ---------------------------------------------------------------------------
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

  // [Bugfix] 参数严格校验
  if (!std::isfinite(density_) || !std::isfinite(youngsModulus_) ||
      !std::isfinite(poissonsRatio_) || density_ <= 0.0 ||
      youngsModulus_ <= 0.0 || poissonsRatio_ < 0.0 ||
      poissonsRatio_ >= 0.5) {
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
    LOG_ERROR("[J2VoceLemaitreMat] LemaitreS must be strictly positive: " + name_);
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

  fm.addField(reg.createField("DoubleField", "PlasticStrain_Old", 9));
  fm.addField(reg.createField("DoubleField", "PlasticStrain_Trial", 9));
  fm.registerSwapPair("PlasticStrain_Old", "PlasticStrain_Trial");

  // 损伤场
  fm.addField(reg.createField("DoubleField", "Damage_Old", 1));
  fm.addField(reg.createField("DoubleField", "Damage_Trial", 1));
  fm.registerSwapPair("Damage_Old", "Damage_Trial");

  fm.addField(reg.createField("DoubleField", "VonMisesStress", 1));
}

void J2VoceLemaitreMat::bindStateVariables(PDCommon::Field::FieldManager &fm) {
  numParticles_ = fm.getFieldAs<double>("EqPlasticStrain_Old")->size();

  fieldEqPSOld_ = fm.getFieldAs<double>("EqPlasticStrain_Old");
  fieldEqPSTrial_ = fm.getFieldAs<double>("EqPlasticStrain_Trial");
  fieldPSOld_ = fm.getFieldAs<double>("PlasticStrain_Old");
  fieldPSTrial_ = fm.getFieldAs<double>("PlasticStrain_Trial");
  fieldDamageOld_ = fm.getFieldAs<double>("Damage_Old");
  fieldDamageTrial_ = fm.getFieldAs<double>("Damage_Trial");
  vonMises_ = fm.getFieldAs<double>("VonMisesStress")->dataPtr();

  eqPSOld_ = fieldEqPSOld_->dataPtr();
  eqPSTrial_ = fieldEqPSTrial_->dataPtr();
  pSOld_ = fieldPSOld_->dataPtr();
  pSTrial_ = fieldPSTrial_->dataPtr();
  damageOld_ = fieldDamageOld_->dataPtr();
  damageTrial_ = fieldDamageTrial_->dataPtr();
}

size_t J2VoceLemaitreMat::getNumStateVariables() const { return 0; }

void J2VoceLemaitreMat::evaluate() {
  // 不使用标准 evaluate，采用 ComputePK1Stress 显式调用
}

void J2VoceLemaitreMat::commitState() {
  if (!fieldEqPSOld_ || !fieldEqPSTrial_ || !fieldPSOld_ || !fieldPSTrial_)
    return;

  eqPSOld_ = fieldEqPSOld_->dataPtr();
  eqPSTrial_ = fieldEqPSTrial_->dataPtr();
  pSOld_ = fieldPSOld_->dataPtr();
  pSTrial_ = fieldPSTrial_->dataPtr();

  if (fieldDamageOld_ && fieldDamageTrial_) {
    damageOld_ = fieldDamageOld_->dataPtr();
    damageTrial_ = fieldDamageTrial_->dataPtr();
  }
}

Eigen::Matrix3d J2VoceLemaitreMat::ComputeEngineeringStress(
    const Eigen::Matrix3d &strain) const {
  double e_trace = strain.trace();
  Eigen::Matrix3d stress =
      2.0 * mu_ * strain + lambda_ * e_trace * Eigen::Matrix3d::Identity();
  return stress;
}

Eigen::Matrix3d J2VoceLemaitreMat::ComputePK1Stress(const Eigen::Matrix3d &F,
                                                    int particleId,
                                                    int stateMode) const {
  if (!F.allFinite()) {
    return Eigen::Matrix3d::Zero();
  }

  if (particleId < 0 || eqPSOld_ == nullptr) {
    Eigen::Matrix3d strain =
        0.5 * (F + F.transpose()) - Eigen::Matrix3d::Identity();
    return ComputeEngineeringStress(strain);
  }

  Eigen::Matrix3d I = Eigen::Matrix3d::Identity();
  Eigen::Matrix3d eps = Eigen::Matrix3d::Zero();
  constexpr double minW = 0.001;

  if (largeDeformation_) {
    eps = 0.5 * (F.transpose() * F) - I;
  } else {
    eps = 0.5 * (F + F.transpose()) - I;
  }

  int idx9 = particleId * 9;

  // -------------------------------------------------------------------------
  // 冻结态预测 (Outer Iteration Predictor)
  // -------------------------------------------------------------------------
  if (stateMode == 1 || stateMode == 2) {
    Eigen::Matrix3d eps_p_fixed = Eigen::Matrix3d::Zero();
    const double *ps_ptr = (stateMode == 1) ? pSOld_ : pSTrial_;

    eps_p_fixed << ps_ptr[idx9], ps_ptr[idx9 + 1], ps_ptr[idx9 + 2],
        ps_ptr[idx9 + 3], ps_ptr[idx9 + 4], ps_ptr[idx9 + 5], ps_ptr[idx9 + 6],
        ps_ptr[idx9 + 7], ps_ptr[idx9 + 8];
    if (!eps_p_fixed.allFinite()) {
      eps_p_fixed.setZero();
    }

    double damage_fixed =
        (stateMode == 1) ? damageOld_[particleId] : damageTrial_[particleId];
    damage_fixed = ClampFinite(damage_fixed, 0.0, 1.0 - minW, 0.0);

    Eigen::Matrix3d eps_e = eps - eps_p_fixed;

    // [新增修复] stateMode == 2 的标量切线刚度削弱加速收敛
    double current_mu = mu_;
    double current_lambda = lambda_;

    if (stateMode == 2) {
      if (eqPSTrial_[particleId] > eqPSOld_[particleId] + 1.0e-12) {
        double alpha_cur = ClampFinite(eqPSTrial_[particleId], 0.0,
                                       kMaxEqPlasticStrain, 0.0);
        double H_prime =
            linearHardening_ + voceR_ * voceB_ * SafeExp(-voceB_ * alpha_cur);
        double beta = 1.0;
        const double denom = H_prime + 3.0 * mu_;
        if (std::isfinite(H_prime) && std::isfinite(denom) &&
            std::abs(denom) > 1.0e-30) {
          beta = H_prime / denom;
        }
        beta = std::clamp(beta, 0.05, 1.0); // 保留至少5%剪切刚度防止奇异

        current_mu = mu_ * beta;
        double K_bulk = lambda_ + (2.0 / 3.0) * mu_;
        current_lambda = K_bulk - (2.0 / 3.0) * current_mu;
      }
    }

    double e_trace = eps_e.trace();
    Eigen::Matrix3d S_pred =
        2.0 * current_mu * eps_e + current_lambda * e_trace * I;

    Eigen::Matrix3d nominal_S_pred = (1.0 - damage_fixed) * S_pred;
    if (!nominal_S_pred.allFinite()) {
      return Eigen::Matrix3d::Zero();
    }

    if (largeDeformation_) {
      Eigen::Matrix3d P_pred = F * nominal_S_pred;
      return P_pred.allFinite() ? P_pred : Eigen::Matrix3d::Zero();
    }
    return nominal_S_pred;
  }

  // =========================================================================
  // Returnmap.cpp 对齐路径：J2-Voce-Lemaitre 返回映射
  // =========================================================================
  constexpr double tol = 1.0e-8;
  constexpr int maxIter = 200;

  Eigen::Matrix3d eps_p_old = Eigen::Matrix3d::Zero();
  eps_p_old << pSOld_[idx9], pSOld_[idx9 + 1], pSOld_[idx9 + 2],
      pSOld_[idx9 + 3], pSOld_[idx9 + 4], pSOld_[idx9 + 5], pSOld_[idx9 + 6],
      pSOld_[idx9 + 7], pSOld_[idx9 + 8];
  if (!eps_p_old.allFinite()) {
    eps_p_old.setZero();
  }

  const double alpha_old = ClampFinite(eqPSOld_[particleId], 0.0,
                                       kMaxEqPlasticStrain, 0.0);
  const double damage_old =
      ClampFinite(damageOld_[particleId], 0.0, 1.0 - minW, 0.0);
  double Wn = std::clamp(1.0 - damage_old, minW, 1.0);

  auto yieldStressAt = [this](double alpha) {
    alpha = ClampFinite(alpha, 0.0, kMaxEqPlasticStrain, 0.0);
    return yieldStress_ + linearHardening_ * alpha +
           voceR_ * (1.0 - SafeExp(-voceB_ * alpha));
  };

  auto hardeningAt = [this](double alpha) {
    alpha = ClampFinite(alpha, 0.0, kMaxEqPlasticStrain, 0.0);
    return linearHardening_ + voceR_ * voceB_ * SafeExp(-voceB_ * alpha);
  };

  Eigen::Matrix3d eps_e_trial = eps - eps_p_old;
  const double tr_eps_e = eps_e_trial.trace();
  const double s_m = bulkModulus_ * tr_eps_e;
  Eigen::Matrix3d s_trial = ComputeEngineeringStress(eps_e_trial);
  Eigen::Matrix3d s_dev_trial = s_trial - s_m * I;

  const double dev_norm =
      std::sqrt((s_dev_trial.cwiseProduct(s_dev_trial)).sum());
  const double J2 = std::sqrt(1.5) * dev_norm;
  if (!std::isfinite(J2) || !std::isfinite(s_m) || !s_trial.allFinite()) {
    damageTrial_[particleId] = damage_old;
    eqPSTrial_[particleId] = alpha_old;
    for (int n = 0; n < 9; ++n) {
      pSTrial_[idx9 + n] = eps_p_old(n / 3, n % 3);
    }
    vonMises_[particleId] = 0.0;
    return Eigen::Matrix3d::Zero();
  }

  double gamma = 0.0;
  double W = Wn;
  double alpha_new = alpha_old;
  double SY = yieldStressAt(alpha_old);

  Eigen::Matrix3d sigma_nom = W * s_trial;
  Eigen::Matrix3d eps_p_new = eps_p_old;
  double mises_nom = W * J2;

  double f = J2 - 3.0 * mu_ * gamma / Wn - SY;
  auto solvePlasticOnly = [&](double initialGamma, double fixedW,
                              double &gammaOut, double &alphaOut) {
    fixedW = ClampFinite(fixedW, minW, 1.0, Wn);
    const double maxGamma =
        std::max(0.0, kMaxEqPlasticStrain - alpha_old);
    gammaOut = ClampFinite(initialGamma, 0.0, maxGamma, 0.0);
    alphaOut = alpha_old + gammaOut;
    double res = 0.0;

    for (int iter = 0; iter < maxIter; ++iter) {
      const double sy = yieldStressAt(alphaOut);
      res = J2 - 3.0 * mu_ * gammaOut / fixedW - sy;
      if (!std::isfinite(res)) {
        return false;
      }
      if (std::abs(res) <= tol) {
        return true;
      }

      const double H = hardeningAt(alphaOut);
      const double denom = -3.0 * mu_ / fixedW - H;
      if (!std::isfinite(H) || !std::isfinite(denom) ||
          std::abs(denom) < 1.0e-30) {
        return false;
      }

      const double dGamma = -res / denom;
      if (!std::isfinite(dGamma)) {
        return false;
      }
      gammaOut = ClampFinite(gammaOut + dGamma, 0.0, maxGamma, gammaOut);
      alphaOut = alpha_old + gammaOut;
    }

    return std::isfinite(res);
  };

  if (f > tol && J2 > 1.0e-16) {
    const double maxGamma =
        std::max(0.0, kMaxEqPlasticStrain - alpha_old);
    bool plasticUpdateOk = true;

    gamma = ClampFinite(Wn * f / (3.0 * mu_), 0.0, maxGamma, 0.0);
    alpha_new = alpha_old + gamma;

    if (tr_eps_e >= 0.0) {
      double res1 = 1.0;
      double res2 = 1.0;
      double normRes = std::sqrt(res1 * res1 + res2 * res2);
      int iter = 1;
      bool coupledConverged = false;

      while (normRes > tol && iter <= maxIter) {
        W = ClampFinite(W, minW, Wn, Wn);
        gamma = ClampFinite(gamma, 0.0, maxGamma, 0.0);
        alpha_new = alpha_old + gamma;

        const double q = W * J2 - 3.0 * mu_ * gamma;
        SY = yieldStressAt(alpha_new);

        const double Y =
            -(q * q / (6.0 * mu_ * W * W) + s_m * s_m / (2.0 * bulkModulus_));
        const double f2 = std::max(-Y / lemaitreS_, 0.0);
        double f2s = 0.0;
        double f2sMinus1 = 0.0;
        if (!std::isfinite(q) || !std::isfinite(Y) ||
            !std::isfinite(f2) || !SafePow(f2, lemaitre_s_, f2s)) {
          plasticUpdateOk = false;
          break;
        }
        if (lemaitre_s_ == 1.0) {
          f2sMinus1 = 1.0;
        } else if (!SafePow(f2, lemaitre_s_ - 1.0, f2sMinus1)) {
          plasticUpdateOk = false;
          break;
        }

        res1 = J2 - 3.0 * mu_ * gamma / W - SY;
        res2 = W - Wn + f2s * gamma / W;
        normRes = std::sqrt(res1 * res1 + res2 * res2);
        if (!std::isfinite(res1) || !std::isfinite(res2) ||
            !std::isfinite(normRes)) {
          plasticUpdateOk = false;
          break;
        }
        if (normRes <= tol) {
          coupledConverged = true;
          break;
        }

        const double H = hardeningAt(alpha_new);
        const double H11 = -3.0 * mu_ / W - H;
        const double H12 = 3.0 * mu_ * gamma / (W * W);
        const double H21 =
            (f2s + gamma * lemaitre_s_ * f2sMinus1 * q /
                       (lemaitreS_ * W * W)) /
            W;
        const double H22 =
            1.0 +
            gamma *
                (lemaitre_s_ * f2sMinus1 *
                     (W * q * J2 - q * q) /
                     (lemaitreS_ * 3.0 * mu_ * W * W) -
                 f2s) /
                (W * W);
        const double det = H11 * H22 - H12 * H21;
        if (!std::isfinite(H) || !std::isfinite(H11) ||
            !std::isfinite(H12) || !std::isfinite(H21) ||
            !std::isfinite(H22) || !std::isfinite(det) ||
            std::abs(det) < 1.0e-30) {
          plasticUpdateOk = false;
          break;
        }

        const double dGamma = -(H22 * res1 - H12 * res2) / det;
        const double dW = -(-H21 * res1 + H11 * res2) / det;
        if (!std::isfinite(dGamma) || !std::isfinite(dW)) {
          plasticUpdateOk = false;
          break;
        }

        gamma = ClampFinite(gamma + dGamma, 0.0, maxGamma, gamma);
        W = ClampFinite(W + dW, minW, Wn, W);
        alpha_new = alpha_old + gamma;
        ++iter;
      }

      if (!coupledConverged) {
        plasticUpdateOk = false;
      }
    } else {
      plasticUpdateOk = solvePlasticOnly(gamma, W, gamma, alpha_new);
      W = Wn;
    }

    if (!plasticUpdateOk) {
      double fallbackGamma = gamma;
      double fallbackAlpha = alpha_new;
      if (solvePlasticOnly(gamma, Wn, fallbackGamma, fallbackAlpha)) {
        gamma = fallbackGamma;
        alpha_new = fallbackAlpha;
        W = Wn;
        plasticUpdateOk = true;
      } else {
        gamma = 0.0;
        alpha_new = alpha_old;
        W = Wn;
        sigma_nom = W * s_trial;
        eps_p_new = eps_p_old;
        mises_nom = W * J2;
        plasticUpdateOk = false;
      }
    }

    if (plasticUpdateOk) {
      W = ClampFinite(W, minW, Wn, Wn);
      gamma = ClampFinite(gamma, 0.0, maxGamma, 0.0);
      alpha_new = alpha_old + gamma;
      SY = yieldStressAt(alpha_new);
      const double q_nom = W * SY;
      const double p_nom = W * s_m;
      Eigen::Matrix3d s_dev_new = (q_nom / J2) * s_dev_trial;
      sigma_nom = s_dev_new + p_nom * I;

      Eigen::Matrix3d eps_e_new =
          s_dev_new / (2.0 * mu_ * W) + (p_nom / (3.0 * W * bulkModulus_)) * I;
      eps_p_new = eps_p_old + eps_e_trial - eps_e_new;
      mises_nom = q_nom;
    }
  }

  W = ClampFinite(W, minW, 1.0, Wn);
  alpha_new = ClampFinite(alpha_new, alpha_old, kMaxEqPlasticStrain, alpha_old);
  if (!sigma_nom.allFinite() || !eps_p_new.allFinite() ||
      !std::isfinite(mises_nom)) {
    damageTrial_[particleId] = damage_old;
    eqPSTrial_[particleId] = alpha_old;
    pSTrial_[idx9] = eps_p_old(0, 0);
    pSTrial_[idx9 + 1] = eps_p_old(0, 1);
    pSTrial_[idx9 + 2] = eps_p_old(0, 2);
    pSTrial_[idx9 + 3] = eps_p_old(1, 0);
    pSTrial_[idx9 + 4] = eps_p_old(1, 1);
    pSTrial_[idx9 + 5] = eps_p_old(1, 2);
    pSTrial_[idx9 + 6] = eps_p_old(2, 0);
    pSTrial_[idx9 + 7] = eps_p_old(2, 1);
    pSTrial_[idx9 + 8] = eps_p_old(2, 2);
    vonMises_[particleId] = std::isfinite(J2) ? Wn * J2 : 0.0;

    Eigen::Matrix3d safeStress = Wn * s_trial;
    if (!safeStress.allFinite()) {
      safeStress.setZero();
    }
    if (largeDeformation_) {
      Eigen::Matrix3d P_safe = F * safeStress;
      return P_safe.allFinite() ? P_safe : Eigen::Matrix3d::Zero();
    }
    return safeStress;
  }

  const double damage_new =
      std::clamp(std::max(damage_old, 1.0 - W), 0.0, 1.0 - minW);
  damageTrial_[particleId] = damage_new;
  eqPSTrial_[particleId] = alpha_new;
  pSTrial_[idx9] = eps_p_new(0, 0);
  pSTrial_[idx9 + 1] = eps_p_new(0, 1);
  pSTrial_[idx9 + 2] = eps_p_new(0, 2);
  pSTrial_[idx9 + 3] = eps_p_new(1, 0);
  pSTrial_[idx9 + 4] = eps_p_new(1, 1);
  pSTrial_[idx9 + 5] = eps_p_new(1, 2);
  pSTrial_[idx9 + 6] = eps_p_new(2, 0);
  pSTrial_[idx9 + 7] = eps_p_new(2, 1);
  pSTrial_[idx9 + 8] = eps_p_new(2, 2);
  vonMises_[particleId] = mises_nom;

  Eigen::Matrix3d P1 = sigma_nom;
  if (largeDeformation_) {
    P1 = F * sigma_nom;
  }

  if (!P1.allFinite()) {
    return Eigen::Matrix3d::Zero();
  }
  return P1;
}

} // namespace PDCommon::Material
