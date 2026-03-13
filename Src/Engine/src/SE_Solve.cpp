#include "BCManager.h"
#include "TypedField.h"
#include "FieldManager.h"
#include "Logger.h"
#include "NeighborList.h"
#include "PDSimulater.h"
#include "ParticleManager.h"
#include "SolverEngine.h"
#include "ThermalMat.h"
#include <cmath>
#include <omp.h>
#include <yaml-cpp/yaml.h>


namespace GRPD::Engine {

void SolverEngine::SolvePD() {
  auto &ctx = this->pdContext_;
  LOG_INFO("Starting PD Solver Loop...");

  // =================================================================
  // 1. 获取仿真参数 (从 YAML)
  // =================================================================
  std::string yamlPath = "../../Input/PD.yaml";
  YAML::Node config = YAML::LoadFile(yamlPath);

  double dt = 1e-4;
  double totalTime = 1.0;
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
  auto *fluxField = fieldManager.getFieldAs<double>("HeatFlux");

  auto &neighborList = ctx.getNeighborList();
  auto &manager = ctx.getParticleManager();
  auto &bcManager = ctx.getBCManager();

  size_t numParticles = manager.getTotalParticles();
  double horizon = neighborList.getHorizon();

  // 3D 微导率系数 (Bobaru model): kappa = 6k / (pi * delta^4)
  // 注意：每个点的材料可能不同，需要在循环内获取 k
  double PI = 3.14159265358979323846;
  double horizon4 = std::pow(horizon, 4.0);

  // 获取数据指针 (SoA)
  const double *volumes = manager.getGeomDataPtr(GRPD::Model::ModelVar::Volume);
  const double *coords = manager.getGeomDataPtr(GRPD::Model::ModelVar::Coords);

  // 获取 Field 的裸指针（用于 OpenMP 高性能循环）
  double *tempPtr = tempField->dataPtr();
  double *ratePtr = rateField->dataPtr();
  double *fluxPtr = fluxField->dataPtr();

  LOG_INFO("Time loop: Dt = " + std::to_string(dt) +
           ", TotalSteps = " + std::to_string(totalSteps));

  // =================================================================
  // 3. 时间步循环
  // =================================================================
  for (int step = 0; step <= totalSteps; ++step) {

    // (A) 输出结果
    if (step % outputInterval == 0) {
      LOG_INFO("--- Outputting Step: " + std::to_string(step));
      this->OutputPD();
    }
    if (step == totalSteps)
      break;

    // (B) 清零变化率
    rateField->clearToZero();
    fluxField->clearToZero();

    // (C) 施加所有边界条件中的源项 (含热流、对流)
    // 源项会累加至 TempRate 和 HeatFlux
    bcManager.applySources();
    
    // (C2) 施加强制约束 (Dirichlet) - 第一次施加用于更新初值
    bcManager.applyConstraints();

// (D) 计算 PD 键相互作用 (并行的热传导内核)
#pragma omp parallel for schedule(dynamic, 64)
    for (int i = 0; i < static_cast<int>(numParticles); ++i) {
      const auto &neighbors = neighborList.getNeighbors(i);
      if (neighbors.empty())
        continue;

      // 获取当前粒子材料属性
      auto *mat = dynamic_cast<GRPD::Material::ThermalMat *>(
          manager.getParticle(i).getMaterial());
      if (!mat)
        continue;

      double k = mat->getConductivity();
      double kappa_base = (6.0 * k) / (PI * horizon4);

      double Ti = tempPtr[i];
      const double *pos_i = &coords[3 * i];

      double totalContribution = 0.0;

      for (int j : neighbors) {
        double Tj = tempPtr[j];
        const double *pos_j = &coords[3 * j];

        double dx = pos_j[0] - pos_i[0];
        double dy = pos_j[1] - pos_i[1];
        double dz = pos_j[2] - pos_i[2];
        double dist = std::sqrt(dx * dx + dy * dy + dz * dz);

        if (dist < 1e-12)
          continue; // 避免自耦合或重合点

        // PD 传导公式：kappa * (Tj - Ti) / dist * Vj
        // 注意：有些公式是 / dist^2，这里按 Bobaru (2012)
        // 积分项为 \int kappa * (Tj - Ti)/dist dVj
        double bondContribution = kappa_base * ((Tj - Ti) / dist) * volumes[j];
        totalContribution += bondContribution;
      }

      rateField->add(i, totalContribution);
      fluxField->add(i, totalContribution);
    }

// (E) 显式时间积分：T_new = T_old + (Rate / (rho * Cp)) * dt
#pragma omp parallel for
    for (int i = 0; i < static_cast<int>(numParticles); ++i) {
      auto *mat = dynamic_cast<GRPD::Material::ThermalMat *>(
          manager.getParticle(i).getMaterial());
      if (!mat)
        continue;

      double rho = mat->getDensity();
      double cp = mat->getHeatCapacity();

      // 注意：bcManager 施加的 Flux 是 Q (功率)，
      // PD Kernel 计算出的也是源项率
      // dT/dt = Net_Rate / (rho * Cp)
      double heatRate = ratePtr[i];
      double deltaT = (heatRate / (rho * cp)) * dt;

      tempPtr[i] += deltaT;
    }

    // (F) 施加固定温度约束 (Dirichlet) - 时间积分后重新强制覆盖
    bcManager.applyConstraints();
  }

  LOG_INFO("PD Solver Simulation Finished.");
}

} // namespace GRPD::Engine