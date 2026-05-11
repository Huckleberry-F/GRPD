// ============================================================================
// MatrixFreeImplicitIntegrator.cpp
// ============================================================================

#include "MatrixFreeImplicitIntegrator.h"
#include "TimeIntegratorRegistry.h"
#include "Logger.h"
#include <cmath>
#include <iostream>

namespace Src::Integration {

void MatrixFreeImplicitIntegrator::configure(const YAML::Node &solverNode) {
  TimeIntegrator::configure(solverNode);
  
  if (solverNode["Algorithm"]) {
    algorithm_ = solverNode["Algorithm"].as<std::string>();
  }
  if (solverNode["MaxNRIters"]) maxNRIters_ = solverNode["MaxNRIters"].as<int>();
  if (solverNode["NRForceTol"]) NRForceTol_ = solverNode["NRForceTol"].as<double>();
  if (solverNode["GMRES_MaxIters"]) gmresMaxIters_ = solverNode["GMRES_MaxIters"].as<int>();
  if (solverNode["LBFGS_Memory"]) lbfgsMemory_ = solverNode["LBFGS_Memory"].as<int>();
  
  LOG_INFO("[MatrixFreeImplicit] Configured with Algorithm: " + algorithm_);
}

std::vector<double> MatrixFreeImplicitIntegrator::flattenDisplacement() {
  size_t totalSize = 0;
  for (const auto& so : soTargets_) totalSize += so.totalComponents;
  std::vector<double> u(totalSize, 0.0);
  size_t offset = 0;
  for (const auto& so : soTargets_) {
    std::copy(so.uPtr, so.uPtr + so.totalComponents, u.begin() + offset);
    offset += so.totalComponents;
  }
  return u;
}

void MatrixFreeImplicitIntegrator::unflattenDisplacement(const std::vector<double>& u) {
  size_t offset = 0;
  for (auto& so : soTargets_) {
    std::copy(u.begin() + offset, u.begin() + offset + so.totalComponents, so.uPtr);
    offset += so.totalComponents;
  }
}

std::vector<double> MatrixFreeImplicitIntegrator::evaluateResidual(
    PDCommon::Core::PDContext &ctx, 
    std::vector<std::unique_ptr<PDKernel>> &kernels, 
    const std::vector<double>& u,
    int currentStep, double activeLF) {
  
  auto u_backup = flattenDisplacement();
  unflattenDisplacement(u);
  
  // 核心：强制重置状态并使用 stateMode=0 计算真实的物理内力
  ctx.setStateFrozen(false);
  evaluateForces(ctx, kernels, accFieldNames_, currentStep, activeLF);
  
  size_t totalSize = 0;
  for (const auto& so : soTargets_) totalSize += so.totalComponents;
  std::vector<double> R(totalSize, 0.0);
  size_t offset = 0;
  for (const auto& so : soTargets_) {
    // aPtr 里存的是 F_int + F_ext (或类似组合)，它即是当前的不平衡力残差
    std::copy(so.aPtr, so.aPtr + so.totalComponents, R.begin() + offset);
    offset += so.totalComponents;
  }
  
  unflattenDisplacement(u_backup);
  return R;
}

std::vector<double> MatrixFreeImplicitIntegrator::solveJFNK(
    PDCommon::Core::PDContext &ctx, 
    std::vector<std::unique_ptr<PDKernel>> &kernels, 
    const std::vector<double>& u_k, 
    const std::vector<double>& R_k,
    int currentStep, double activeLF) {
    
  size_t N = u_k.size();
  int m = std::min(gmresMaxIters_, static_cast<int>(N));
  if (m == 0) return std::vector<double>(N, 0.0);
  
  std::vector<std::vector<double>> V(m + 1, std::vector<double>(N, 0.0));
  std::vector<std::vector<double>> H(m + 1, std::vector<double>(m, 0.0));
  std::vector<double> g(m + 1, 0.0);
  std::vector<double> cs(m, 0.0);
  std::vector<double> sn(m, 0.0);
  
  double normR0 = 0.0;
  for (size_t i=0; i<N; ++i) {
    V[0][i] = R_k[i]; // 解 J * du = R_k, 寻找正向残差的平息方向
    normR0 += R_k[i] * R_k[i];
  }
  normR0 = std::sqrt(normR0);
  
  if (normR0 < 1e-16) return std::vector<double>(N, 0.0);
  
  for (size_t i=0; i<N; ++i) V[0][i] /= normR0;
  g[0] = normR0;
  
  int k = 0;
  for (; k < m; ++k) {
    // Matrix-Free Jacobian-Vector Product
    std::vector<double> u_temp(N, 0.0);
    for (size_t i=0; i<N; ++i) u_temp[i] = u_k[i] + fdEpsilon_ * V[k][i];
    
    std::vector<double> R_temp = evaluateResidual(ctx, kernels, u_temp, currentStep, activeLF);
    
    std::vector<double> w(N, 0.0);
    for (size_t i=0; i<N; ++i) {
      w[i] = -(R_temp[i] - R_k[i]) / fdEpsilon_; // Jv 近似 (注意符号调整)
    }
    
    // Arnoldi 过程
    for (int j = 0; j <= k; ++j) {
      double dot = 0.0;
      for (size_t i=0; i<N; ++i) dot += w[i] * V[j][i];
      H[j][k] = dot;
      for (size_t i=0; i<N; ++i) w[i] -= dot * V[j][i];
    }
    
    double normW = 0.0;
    for (size_t i=0; i<N; ++i) normW += w[i]*w[i];
    normW = std::sqrt(normW);
    H[k+1][k] = normW;
    
    if (normW > 1e-16) {
      for (size_t i=0; i<N; ++i) V[k+1][i] = w[i] / normW;
    }
    
    // Givens 旋转应用
    for (int j = 0; j < k; ++j) {
      double temp = cs[j] * H[j][k] + sn[j] * H[j+1][k];
      H[j+1][k] = -sn[j] * H[j][k] + cs[j] * H[j+1][k];
      H[j][k] = temp;
    }
    
    double beta = std::sqrt(H[k][k]*H[k][k] + H[k+1][k]*H[k+1][k]);
    if (beta > 1e-16) {
      cs[k] = H[k][k] / beta;
      sn[k] = H[k+1][k] / beta;
    } else {
      cs[k] = 1.0; sn[k] = 0.0;
    }
    
    H[k][k] = beta;
    H[k+1][k] = 0.0;
    
    g[k+1] = -sn[k] * g[k];
    g[k] = cs[k] * g[k];
    
    double error = std::abs(g[k+1]);
    if (error / normR0 < gmresTol_) {
      k++;
      break;
    }
  }
  
  // 回代求解 y
  std::vector<double> y(k, 0.0);
  for (int i = k - 1; i >= 0; --i) {
    y[i] = g[i];
    for (int j = i + 1; j < k; ++j) y[i] -= H[i][j] * y[j];
    if (std::abs(H[i][i]) > 1e-16) y[i] /= H[i][i];
  }
  
  // 组合得到位移修正量
  std::vector<double> delta_u(N, 0.0);
  for (int j = 0; j < k; ++j) {
    for (size_t i=0; i<N; ++i) delta_u[i] += V[j][i] * y[j];
  }
  
  return delta_u;
}

std::vector<double> MatrixFreeImplicitIntegrator::solveLBFGS(
    PDCommon::Core::PDContext &ctx, 
    std::vector<std::unique_ptr<PDKernel>> &kernels, 
    const std::vector<double>& u_k, 
    const std::vector<double>& R_k,
    int currentStep, double activeLF) {
    
  size_t N = u_k.size();
  std::vector<double> q = R_k; 
  
  int m = s_history_.size();
  std::vector<double> alpha(m, 0.0);
  
  // Two-loop recursion: 向后遍历
  for (int i = m - 1; i >= 0; --i) {
    double dot = 0.0;
    for (size_t j=0; j<N; ++j) dot += s_history_[i][j] * q[j];
    alpha[i] = rho_history_[i] * dot;
    for (size_t j=0; j<N; ++j) q[j] -= alpha[i] * y_history_[i][j];
  }
  
  // H0 缩放
  double gamma = 1.0;
  if (m > 0) {
    double dot_sy = 0.0, dot_yy = 0.0;
    for (size_t j=0; j<N; ++j) {
      dot_sy += s_history_.back()[j] * y_history_.back()[j];
      dot_yy += y_history_.back()[j] * y_history_.back()[j];
    }
    if (dot_yy > 1e-16) gamma = dot_sy / dot_yy;
  }
  
  std::vector<double> z(N, 0.0);
  for (size_t j=0; j<N; ++j) z[j] = gamma * q[j];
  
  // Two-loop recursion: 向前遍历
  for (int i = 0; i < m; ++i) {
    double dot = 0.0;
    for (size_t j=0; j<N; ++j) dot += y_history_[i][j] * z[j];
    double beta = rho_history_[i] * dot;
    for (size_t j=0; j<N; ++j) z[j] += s_history_[i][j] * (alpha[i] - beta);
  }
  
  // L-BFGS 给出的 z = H_k * R_k，修正量应为 z (假设 R_k 是残差力，需要向顺着力的方向移动)
  std::vector<double> delta_u(N, 0.0);
  for (size_t j=0; j<N; ++j) delta_u[j] = z[j]; 
  return delta_u;
}

void MatrixFreeImplicitIntegrator::run(PDCommon::Core::PDContext &ctx,
                                       std::vector<std::unique_ptr<PDKernel>> &kernels,
                                       std::function<void(int, double)> outputCallback) {
  extractSecondOrderTargets(kernels, ctx, soTargets_, accFieldNames_);
  if (soTargets_.empty()) {
    LOG_ERROR("[MatrixFreeImplicit] No second-order targets found.");
    return;
  }

  double prevTargetTime = 0.0;
  
  for (const auto& config : loadStepConfigs_) {
    for (int sub = 1; sub <= config.numSubsteps; ++sub) {
      double activeLF = 1.0;
      if (kbc_ == 0) { // Ramp
        double subFraction = static_cast<double>(sub) / config.numSubsteps;
        double currentTime = prevTargetTime + subFraction * (config.targetTime - prevTargetTime);
        activeLF = currentTime / totalTime_; 
      } else { // Step
        activeLF = config.targetTime / totalTime_;
      }
      
      LOG_INFO("==========================================================");
      LOG_INFO("[MatrixFreeImplicit] Step " + std::to_string(config.stepId) + " / Substep " + std::to_string(sub) + " | LF=" + std::to_string(activeLF));

      // 每一子步开始前清空 L-BFGS 历史，避免跨步的非连续状态污染
      s_history_.clear();
      y_history_.clear();
      rho_history_.clear();

      bool converged = false;
      for (int iter = 0; iter < maxNRIters_; ++iter) {
        auto u_k = flattenDisplacement();
        auto R_k = evaluateResidual(ctx, kernels, u_k, config.stepId, activeLF);
        
        // 简单 L2 范数收敛准则
        double normR = 0.0;
        double maxR = 0.0;
        for (double r : R_k) {
            normR += r*r;
            if (std::abs(r) > maxR) maxR = std::abs(r);
        }
        normR = std::sqrt(normR);
        
        LOG_INFO("  -> Iter " + std::to_string(iter) + " | Res Norm: " + std::to_string(normR) + " | Max Res: " + std::to_string(maxR));
        
        if (normR < NRForceTol_ || maxR < NRForceTol_ * 0.1) {
           LOG_INFO("  [*] Converged in " + std::to_string(iter) + " iterations.");
           converged = true;
           break;
        }

        std::vector<double> delta_u;
        if (algorithm_ == "JFNK") {
            delta_u = solveJFNK(ctx, kernels, u_k, R_k, config.stepId, activeLF);
        } else {
            delta_u = solveLBFGS(ctx, kernels, u_k, R_k, config.stepId, activeLF);
        }

        // 行搜索与位移更新
        double alpha = 1.0;
        std::vector<double> u_new(u_k.size());
        for(size_t i=0; i<u_k.size(); ++i) u_new[i] = u_k[i] + alpha * delta_u[i];
        
        // 如果是 L-BFGS，准备计算 y_k
        if (algorithm_ == "L-BFGS") {
          auto R_new = evaluateResidual(ctx, kernels, u_new, config.stepId, activeLF);
          std::vector<double> s_k(u_k.size());
          std::vector<double> y_k(R_k.size());
          double dot_sy = 0.0;
          
          for(size_t i=0; i<u_k.size(); ++i) {
             s_k[i] = alpha * delta_u[i];
             // y_k 是力的变化。由于 R_k 是剩余力，当移向平衡时，力应该发生变化。
             // 如果残差定义为 F_int + F_ext，为了使逆矩阵正定，这里采用负残差变化。
             y_k[i] = -(R_new[i] - R_k[i]); 
             dot_sy += s_k[i] * y_k[i];
          }
          
          if (dot_sy > 1e-14) {
            s_history_.push_back(s_k);
            y_history_.push_back(y_k);
            rho_history_.push_back(1.0 / dot_sy);
            if (s_history_.size() > lbfgsMemory_) {
               s_history_.pop_front();
               y_history_.pop_front();
               rho_history_.pop_front();
            }
          }
        }
        
        unflattenDisplacement(u_new);
      }
      
      if (!converged) {
         LOG_WARNING("[MatrixFreeImplicit] Substep did not converge within MaxNRIters.");
      }
      
      outputCallback(config.stepId, activeLF);
    }
    prevTargetTime = config.targetTime;
  }
}

} // namespace Src::Integration

REGISTER_TIME_INTEGRATOR(MatrixFreeImplicit, Src::Integration::MatrixFreeImplicitIntegrator)
