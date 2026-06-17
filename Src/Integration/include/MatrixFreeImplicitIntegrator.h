// ============================================================================
// MatrixFreeImplicitIntegrator.h - JFNK & L-BFGS Matrix-Free Solver
//
// 职责：
//   实现纯隐式的非线性方程组求解，摒弃显式虚时间步和动态阻尼。
//   直接求解方程 R(u) = F_int(u) + F_ext = 0。
//   提供两种极其强悍的免组装矩阵（Matrix-Free）算法：
//   1. JFNK (Jacobian-Free Newton-Krylov)
//   2. L-BFGS (Limited-memory BFGS)
//
// 架构位置：
//   继承自 TimeIntegrator，与 ADR_Integrator 平级。
// ============================================================================

#ifndef SRC_INTEGRATION_MATRIX_FREE_IMPLICIT_INTEGRATOR_H
#define SRC_INTEGRATION_MATRIX_FREE_IMPLICIT_INTEGRATOR_H

#include "TimeIntegrator.h"
#include <deque>
#include <vector>
#include <string>

namespace Src::Integration {

class MatrixFreeImplicitIntegrator : public TimeIntegrator {
public:
  MatrixFreeImplicitIntegrator() = default;
  ~MatrixFreeImplicitIntegrator() override = default;

  void configure(const YAML::Node &solverNode) override;

  void run(PDCommon::Core::PDContext &ctx,
           std::vector<std::unique_ptr<PDKernel>> &kernels,
           std::function<void(int, double)> outputCallback) override;

  std::string getName() const override { return "MatrixFreeImplicit"; }

private:
  std::string algorithm_ = "JFNK"; // "JFNK" 或 "L-BFGS"
  
  // 宏观牛顿迭代控制
  int maxNRIters_ = 50;
  double NRForceTol_ = 5.0e-3;
  double NRDispTol_ = 5.0e-2;
  double minRefForce_ = 1.0e-2;

  // JFNK 专属参数
  int gmresMaxIters_ = 50;
  double gmresTol_ = 1.0e-3;
  double fdEpsilon_ = 1.0e-7;

  // L-BFGS 专属参数
  int lbfgsMemory_ = 10;

  // 内部辅助结构
  std::vector<SecondOrderTarget> soTargets_;
  std::vector<std::string> accFieldNames_;

  std::vector<FirstOrderTarget> foTargets_;
  std::vector<std::string> rateFieldNames_;
  bool isFirstOrder_ = false;

  // --- 辅助计算函数 ---
  
  /// @brief 将所有物理场中的目标位移压平为一维向量
  std::vector<double> flattenDisplacement();
  
  /// @brief 将一维向量写回到各个物理场中的位移
  void unflattenDisplacement(const std::vector<double>& u);
  
  /// @brief 在指定位移下计算系统残差 (R = F_int + F_ext)
  std::vector<double> evaluateResidual(PDCommon::Core::PDContext &ctx, 
                                       std::vector<std::unique_ptr<PDKernel>> &kernels, 
                                       const std::vector<double>& u,
                                       int currentStep,
                                       double activeLF,
                                       bool frozen = false);

  /// @brief 线搜索 (Backtracking Line Search)
  double lineSearch(PDCommon::Core::PDContext &ctx, 
                    std::vector<std::unique_ptr<PDKernel>> &kernels, 
                    const std::vector<double>& u_old, 
                    const std::vector<double>& R_old, 
                    const std::vector<double>& delta_u,
                    int currentStep,
                    double activeLF);

  // --- 核心算法实现 ---
  
  std::vector<double> solveJFNK(PDCommon::Core::PDContext &ctx, 
                                std::vector<std::unique_ptr<PDKernel>> &kernels, 
                                const std::vector<double>& u_k, 
                                const std::vector<double>& R_k,
                                int currentStep,
                                double activeLF);

  std::vector<double> solveLBFGS(PDCommon::Core::PDContext &ctx, 
                                 std::vector<std::unique_ptr<PDKernel>> &kernels, 
                                 const std::vector<double>& u_k, 
                                 const std::vector<double>& R_k,
                                 int currentStep,
                                 double activeLF);

  // L-BFGS 的历史存储队列
  std::deque<std::vector<double>> s_history_;
  std::deque<std::vector<double>> y_history_;
  std::deque<double> rho_history_;
};

} // namespace Src::Integration

#endif // SRC_INTEGRATION_MATRIX_FREE_IMPLICIT_INTEGRATOR_H
