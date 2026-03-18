// ============================================================================
// PD_Solve.cpp - PD 热传导求解核心（NOSB-PD 非常规态基版本）
// ============================================================================

#include "BCManager.h"
#include "FieldManager.h"
#include "Logger.h"
#include "NOSB_T.h"
#include "NeighborList.h"
#include "PDSolver.h"
#include "ParticleManager.h"
#include "TypedField.h"
#include <cmath>
#include <chrono>
#include <omp.h>
#include <yaml-cpp/yaml.h>

namespace Src::Solve {

void PDSolver::Solve() {
  auto &ctx = pdContext_;
  LOG_INFO("Starting PD Solver Loop...");

  // =================================================================
  // 1. 获取仿真参数 (从 YAML)
  // =================================================================
  YAML::Node config = YAML::LoadFile(yamlPath_);

  double dt = 1.0;
  double totalTime = 100.0;
  int outputInterval = 10;

  if (config["Solver"]) {
    if (config["Solver"]["TimeStep_dt"])
      dt = config["Solver"]["TimeStep_dt"].as<double>();
    if (config["Solver"]["TotalTime"])
      totalTime = config["Solver"]["TotalTime"].as<double>();
    if (config["Solver"]["OutputInterval"])
      outputInterval = config["Solver"]["OutputInterval"].as<int>();
  }

  int totalSteps = static_cast<int>(totalTime / dt);

  // =================================================================
  // 2. 获取物理场指针（从 FieldManager）
  // =================================================================
  auto &fieldManager = ctx.getFieldManager();

  if (!fieldManager.hasField("Temperature")) {
    LOG_ERROR("No Temperature field found! Aborting solver.");
    return;
  }

  auto *tempField = fieldManager.getFieldAs<double>("Temperature");
  auto *rateField = fieldManager.getFieldAs<double>("TempRate");

  auto &bcManager = ctx.getBCManager();
  size_t numParticles = ctx.getParticleManager().getTotalParticles();

  // 获取 Field 的裸指针（用于 OpenMP 高性能循环）
  double *tempPtr = tempField->dataPtr();
  double *ratePtr = rateField->dataPtr();

  LOG_INFO("Time loop: Dt = " + std::to_string(dt) +
           ", TotalSteps = " + std::to_string(totalSteps));

  // =================================================================
  // 3. 预计算：NOSB 形状张量 + 注册工作场（仅一次）
  // =================================================================
  NOSB_T nosb;
  nosb.ComputeShapeTensors(ctx);

  // =================================================================
  // 4. 时间步循环
  // =================================================================
  auto tStart = std::chrono::high_resolution_clock::now();
  double pureComputeTime = 0.0; // 累计纯计算时间 (跳过 Output)

  for (int step = 0; step <= totalSteps; ++step) {

    // (A) 输出结果 + 打印进度 (这部分不计入 ComputeTime)
    if (step % outputInterval == 0) {
      auto tNow = std::chrono::high_resolution_clock::now();
      double totalElapsed = std::chrono::duration<double>(tNow - tStart).count();
      
      // 真实纯计算步速 (排除写磁盘的时间)
      double pureSpeed = (step > 0 && pureComputeTime > 0.0) 
                         ? (step / pureComputeTime) : 0.0;
                         
      LOG_INFO("--- Step " + std::to_string(step) + " / " + std::to_string(totalSteps) +
               "  |  Pure Compute: " + std::to_string(pureComputeTime) + "s" +
               "  |  Total: " + std::to_string(totalElapsed) + "s" +
               "  |  Pure Speed: " + std::to_string(pureSpeed) + " steps/s");
               
      LOG_INFO("Starting data export process...");
      this->Output();
    }
    if (step == totalSteps)
      break;

    auto tComputeStart = std::chrono::high_resolution_clock::now();

    // (B) 清零变化率
    rateField->clearToZero();

    // (C) 施加所有边界条件中的源项 (含热流、对流)
    bcManager.applySources();

    // (C2) 施加强制约束 (Dirichlet)
    bcManager.applyConstraints();

    // (D) NOSB-PD 热传导内核（三步非局部积分 + 本构黑盒）
    nosb.ComputeThermalState(ctx);

    // (E) 显式时间积分：T_new = T_old + Rate * dt
    //     注意：NOSB_T 输出的 TempRate 已包含 1/(ρcₚ) 因子
#pragma omp parallel for schedule(static)
    for (int i = 0; i < static_cast<int>(numParticles); ++i) {
      tempPtr[i] += ratePtr[i] * dt;
    }

    // (F) 施加固定温度约束 (Dirichlet) - 积分后重新覆盖
    bcManager.applyConstraints();
    
    // 累加单步的纯计算时间
    auto tComputeEnd = std::chrono::high_resolution_clock::now();
    pureComputeTime += std::chrono::duration<double>(tComputeEnd - tComputeStart).count();
  }

  auto tEnd = std::chrono::high_resolution_clock::now();
  double totalElapsed = std::chrono::duration<double>(tEnd - tStart).count();
  LOG_INFO("PD Solver Finished. Total: " + std::to_string(totalElapsed) + "s" +
           "  |  Pure Compute avg: " + std::to_string(pureComputeTime / totalSteps) + " s/step");
}

} // namespace Src::Solve
