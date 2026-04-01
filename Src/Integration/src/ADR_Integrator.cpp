// ============================================================================
// ADR_Integrator.cpp - ADR / Explicit Quasi-Static Integrator (Verlet scheme)
// ============================================================================

#include "ADR_Integrator.h"
#include "BCManager.h"
#include "FieldManager.h"
#include "Logger.h"
#include "PDKernel.h"
#include "ParticleManager.h"
#include "MaterialManager.h"
#include "Material.h"
#include "MechanicalMaterial.h"
#include "TimeIntegratorRegistry.h"
#include <chrono>
#include <cmath>
#include <omp.h>

namespace Src::Integration {

void ADR_Integrator::configure(const YAML::Node &solverNode) {
  TimeIntegrator::configure(solverNode);

  if (solverNode["NumLoadSteps"])
    numLoadSteps_ = solverNode["NumLoadSteps"].as<int>();
  if (solverNode["NumSubsteps"])
    numSubsteps_ = solverNode["NumSubsteps"].as<int>();
  if (solverNode["KBC"])
    kbc_ = solverNode["KBC"].as<int>();
  if (solverNode["MaxPseudoSteps"])
    maxPseudoSteps_ = solverNode["MaxPseudoSteps"].as<int>();
  if (solverNode["DispTol"])
    dispTol_ = solverNode["DispTol"].as<double>();
  if (solverNode["ForceTol"])
    forceTol_ = solverNode["ForceTol"].as<double>();
}

void ADR_Integrator::run(PDCommon::Core::PDContext &ctx,
                         std::vector<std::unique_ptr<PDKernel>> &kernels,
                         std::function<void(int, double)> outputCallback) {
  const double dt = dt_;
  
  auto &fieldManager = ctx.getFieldManager();
  auto &bcManager = ctx.getBCManager();
  const size_t numParticles = ctx.getParticleManager().getTotalParticles();

  std::vector<PDKernel::IntegrationTarget> targets;
  for (auto &kernel : kernels) {
    auto t = kernel->getIntegrationTargets();
    targets.insert(targets.end(), t.begin(), t.end());
  }

  // Find pure mechanical targets.
  struct SecondOrderTarget {
    double *uPtr;
    double *vPtr;
    double *aPtr;
    size_t totalComponents;
    int dimension;
  };
  std::vector<SecondOrderTarget> soTargets;
  std::vector<std::string> accFieldNames;

  for (const auto &tb : targets) {
    for (const auto &ta : targets) {
      if (tb.rateField == ta.primaryField && tb.dimension == ta.dimension) {
        auto *uField = fieldManager.getFieldAs<double>(tb.primaryField);
        auto *vField = fieldManager.getFieldAs<double>(ta.primaryField);
        auto *aField = fieldManager.getFieldAs<double>(ta.rateField);

        if (uField && vField && aField) {
          soTargets.push_back({
              uField->dataPtr(), vField->dataPtr(), aField->dataPtr(),
              numParticles * tb.dimension, tb.dimension
          });
          accFieldNames.push_back(ta.rateField);
          LOG_INFO("[ADR_Integrator] Found 2nd-order pair: " + tb.primaryField +
                   " <- " + ta.primaryField + " <- " + ta.rateField);
        }
      }
    }
  }

  if (soTargets.empty()) {
    LOG_ERROR("[ADR_Integrator] No 2nd-order ODE pairs found! Cannot run ADR.");
    return;
  }

  for (auto &kernel : kernels) {
    kernel->preCompute(ctx);
  }

  LOG_INFO("[ADR_Integrator] Starting Custom ADR: NumLoadSteps = " +
           std::to_string(numLoadSteps_) + ", NumSubsteps = " + std::to_string(numSubsteps_) +
           ", KBC = " + std::to_string(kbc_));

  // State histories for the specific ADR Leapfrog algorithm
  size_t maxTotalComponents = 0;
  for (const auto &so : soTargets) {
      if (so.totalComponents > maxTotalComponents) maxTotalComponents = so.totalComponents;
  }
  
  std::vector<std::vector<double>> velHalfOld(soTargets.size(), std::vector<double>(maxTotalComponents, 0.0));
  std::vector<std::vector<double>> aOld(soTargets.size(), std::vector<double>(maxTotalComponents, 0.0));
  std::vector<std::vector<double>> dispOld(soTargets.size(), std::vector<double>(maxTotalComponents, 0.0));

  unsigned long long global_tt_d = 1; // Tracks the first iteration across the entire explicit session

  for (int step = 1; step <= numLoadSteps_; ++step) {
    LOG_INFO("=== Load Step " + std::to_string(step) + " / " + std::to_string(numLoadSteps_) + " ===");

    for (int sub = 1; sub <= numSubsteps_; ++sub) {
        // [1] Determine Load Factor based on ANSYS KBC setting
        double targetLF = static_cast<double>(step) / numLoadSteps_;
        double prevLF   = static_cast<double>(step - 1) / numLoadSteps_;
        double loadFactor = prevLF;
        
        if (kbc_ == 0) { // KBC, 0 : RAMP (Linear interpolation)
            loadFactor += (targetLF - prevLF) * (static_cast<double>(sub) / numSubsteps_);
        } else { // KBC, 1 : STEP (Step up immediately)
            loadFactor = targetLF; 
        }

        bool converged = false;
        int iter = 0;
        double TOL2_final = 0.0;
        
        while (!converged && iter < maxPseudoSteps_) {
            
            // Backup old displacement for TOL2 tracking
            for (size_t k = 0; k < soTargets.size(); ++k) {
                const auto &so = soTargets[k];
                #pragma omp parallel for schedule(static)
                for (int i = 0; i < static_cast<int>(so.totalComponents); ++i) {
                    dispOld[k][i] = so.uPtr[i];
                }
            }

            // [2] Internal Force & Acceleration evaluation (P_new/m)
            // Kernels will compute F_int and safely convert it to specific acceleration a_new = F_int / m
            bcManager.applyConstraints(loadFactor);
            evaluateForces(ctx, kernels, accFieldNames);

            // [3] Compute Adaptive Damping parameter (cn)
            // cn1 = cn1 - disp^2 * (a_new - a_old) / (dt * velhalfold)
            // cn2 = cn2 + disp^2
            double cn = 0.0;
            double cn1 = 0.0;
            double cn2 = 0.0;
            
            if (global_tt_d > 1) {
                for (size_t k = 0; k < soTargets.size(); ++k) {
                    const auto &so = soTargets[k];
                    double local_cn1 = 0.0;
                    double local_cn2 = 0.0;
                    
                    #pragma omp parallel for reduction(-:local_cn1) reduction(+:local_cn2) schedule(static)
                    for (int i = 0; i < static_cast<int>(so.totalComponents); ++i) {
                        // Skip perfectly constrained nodes conceptually.
                        // In GRPD, rigid constraints yield velHalfOld[i] = 0 physically because constraints wipe velocity.
                        if (std::abs(velHalfOld[k][i]) > 1e-16) {
                            local_cn1 -= so.uPtr[i] * so.uPtr[i] * (so.aPtr[i] - aOld[k][i]) / (dt * velHalfOld[k][i]);
                        }
                        local_cn2 += so.uPtr[i] * so.uPtr[i];
                    }
                    cn1 += local_cn1;
                    cn2 += local_cn2;
                }
            }
            
            if (cn2 > 1e-30 && (cn1 / cn2) > 0.0) {
                cn = 2.0 * std::sqrt(cn1 / cn2);
            }
            if (cn > 1.8 / dt) {
                cn = 1.8 / dt; // equivalent to user's 0.9 * 2.0 / dt
            }

            // [4] ADR Leapfrog Integration Update
            for (size_t k = 0; k < soTargets.size(); ++k) {
                const auto &so = soTargets[k];
                #pragma omp parallel for schedule(static)
                for (int i = 0; i < static_cast<int>(so.totalComponents); ++i) {
                    double velHalfNew = 0.0;
                    
                    if (global_tt_d == 1) { // Very first explicit integration tick
                        velHalfNew = 0.5 * dt * so.aPtr[i];
                    } else {
                        velHalfNew = ((2.0 - cn * dt) * velHalfOld[k][i] + 2.0 * dt * so.aPtr[i]) / (2.0 + cn * dt);
                    }
                    
                    so.vPtr[i] = 0.5 * (velHalfOld[k][i] + velHalfNew);
                    so.uPtr[i] = so.uPtr[i] + velHalfNew * dt;
                    
                    velHalfOld[k][i] = velHalfNew;
                    aOld[k][i] = so.aPtr[i];
                }
            }
            
            // [5] Re-apply constraints to tightly freeze displacement boundaries
            bcManager.applyConstraints(loadFactor);
            
            // Allow models to compute damage or internal variables
            for (auto &kernel : kernels) {
                kernel->postCompute(ctx);
            }

            // [6] Convergence Check (TOL2 & TOL3)
            double TOL2 = 0.0;
            double TOL3 = 0.0;
            for (size_t k = 0; k < soTargets.size(); ++k) {
                const auto &so = soTargets[k];
                double local_tol2 = 0.0;
                double local_tol3 = 0.0;
                
                #pragma omp parallel for reduction(+:local_tol2, local_tol3) schedule(static)
                for (int i = 0; i < static_cast<int>(so.totalComponents); ++i) {
                    double du = so.uPtr[i] - dispOld[k][i];
                    local_tol2 += du * du;
                    local_tol3 += so.aPtr[i] * so.aPtr[i];
                    // Here aPtr^2 is highly correlated to (pForce/mass)^2.
                }
                TOL2 += local_tol2;
                TOL3 += local_tol3;
            }
            
            TOL2 = std::sqrt(TOL2);
            TOL3 = std::sqrt(TOL3);
            TOL2_final = TOL2;

            // Converged if the displacement increment vector is zero and acceleration residual is low
            if (TOL2 < dispTol_ && iter > 20) {
                converged = true;
                LOG_INFO("    [Sub " + std::to_string(sub) + "] Converged. Iter: " + std::to_string(iter) +
                         " | TOL2(dU): " + std::to_string(TOL2) + " | TOL3(Res): " + std::to_string(TOL3) + " | cn: " + std::to_string(cn));
            }

            if (iter % 1000 == 0 && iter > 0) {
                LOG_INFO("    > Sub " + std::to_string(sub) + " Iter " + std::to_string(iter) + 
                         " | LF: " + std::to_string(loadFactor) + 
                         " | TOL2: " + std::to_string(TOL2) + " | cn: " + std::to_string(cn));
            }

            iter++;
            global_tt_d++;
        }

        if (!converged) {
            LOG_WARNING("    [Sub " + std::to_string(sub) + "] Reached MaxPseudoSteps without convergence! TOL2 = " + std::to_string(TOL2_final));
        }

        // [Crucial] Substep is stable, finalize specific state transitions (like plastic strains)
        auto &matMap = ctx.getMaterialManager().getMaterials();
        for (auto &[matName, matPtr] : matMap) {
            if (matPtr) matPtr->commitState();
        }

        // Typically output after every converged substep, using a pseudo-time or load factor as display time
        if (outputCallback) {
            outputCallback(step * numSubsteps_ + sub, loadFactor);
        }
    }
  }
}

} // namespace Src::Integration

REGISTER_TIME_INTEGRATOR(ADR, Src::Integration::ADR_Integrator)
