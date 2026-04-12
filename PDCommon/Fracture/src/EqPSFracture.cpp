#include "EqPSFracture.h"
#include "FieldManager.h"
#include "FractureRegistry.h"
#include "Logger.h"
#include "NeighborList.h"
#include "PDContext.h"
#include "ParticleManager.h"
#include <omp.h>

REGISTER_FRACTURE_MODEL(EqPSFracture, PDCommon::Fracture::EqPSFracture)

namespace PDCommon::Fracture {

void EqPSFracture::configure(const YAML::Node &node) {
  if (node["Critical_EqPS"]) {
    criticalEqPS_ = node["Critical_EqPS"].as<double>();
  } else {
    criticalEqPS_ = 0.5;
  }

  // 【新增】解析独立配置的宏观损伤阈值
  if (node["Critical_Damage"]) {
    criticalDamage_ = node["Critical_Damage"].as<double>();
  } else {
    criticalDamage_ = 0.8; // 默认：80% 键断裂视作整体破片失活
  }

  LOG_INFO("[EqPSFracture] Configured Critical_EqPS = " +
           std::to_string(criticalEqPS_) +
           ", Critical_Damage = " + std::to_string(criticalDamage_));
}

void EqPSFracture::preCompute(PDCommon::Core::PDContext &ctx, int matId) {
  FractureModel::preCompute(ctx, matId);
}

void EqPSFracture::computeFracture(PDCommon::Core::PDContext &ctx, int matId) {
  auto &neighborList = ctx.getNeighborList();
  auto &manager = ctx.getParticleManager();
  auto &fieldManager = ctx.getFieldManager();

  const size_t numParticles = manager.getTotalParticles();
  // EqPlasticStrain_Trial 是由塑性本构计算后抛出的场
  auto *eqPSField = fieldManager.getFieldAs<double>("EqPlasticStrain_Trial");
  auto *activeStatusField = fieldManager.getFieldAs<int>("ActiveStatus");
  auto *bondIntegrityField = fieldManager.getFieldAs<double>("BondIntegrity");
  auto *pk1StressField =
      fieldManager.getFieldAs<double>("PK1Stress"); // 获取应力场进行拉压判定

  if (!eqPSField || !activeStatusField || !bondIntegrityField) {
    return;
  }

  const double *eqPSPtr = eqPSField->dataPtr();
  int *activeStatusPtr = activeStatusField->dataPtr();
  double *bondIntegrityPtr = bondIntegrityField->dataPtr();
  const double *stressPtr =
      pk1StressField ? pk1StressField->dataPtr() : nullptr;
  const auto &particles = manager.getAllParticles();

  // 核心循环：同时判定粒子自己的塑性状态并更新 ActiveStatus，且直接进行断键
#pragma omp parallel for schedule(guided)
  for (int i = 0; i < static_cast<int>(numParticles); ++i) {
    if (matId != -1 && particles[i].getMatId() != matId)
      continue;

    bool isFailed = (eqPSPtr[i] >= criticalEqPS_);

    // 切应力主导三轴度损伤判定：仅在三轴度 eta > -1/3 时才允许断裂
    if (isFailed && stressPtr) {
      int idx9 = i * 9;
      double trace =
          stressPtr[idx9] + stressPtr[idx9 + 4] + stressPtr[idx9 + 8];
      double sigma_m = trace / 3.0;
      double s11 = stressPtr[idx9] - sigma_m;
      double s22 = stressPtr[idx9 + 4] - sigma_m;
      double s33 = stressPtr[idx9 + 8] - sigma_m;
      double s12 = stressPtr[idx9 + 1];
      double s21 = stressPtr[idx9 + 3];
      double s13 = stressPtr[idx9 + 2];
      double s31 = stressPtr[idx9 + 6];
      double s23 = stressPtr[idx9 + 5];
      double s32 = stressPtr[idx9 + 7];

      double s_ij_s_ij = s11 * s11 + s22 * s22 + s33 * s33 + s12 * s12 +
                         s21 * s21 + s13 * s13 + s31 * s31 + s23 * s23 +
                         s32 * s32;
      double seq = std::sqrt(1.5 * s_ij_s_ij);
      double eta = (seq > 1e-6) ? (sigma_m / seq) : 0.0;

      // 传统 JC 模型判定（已注释）：允许处于极度压迫状态下发生剪切破坏(冲塞)
      // if (eta <= -0.3333333333) {
      //   isFailed = false;
      // }
    }

    // 2. 遍历所有邻接键，当双端均达到阈值时才强制打断并令点失活
    int activeBonds = 0;
    const int numNeighbors = neighborList.getNeighborCount(i);
    int *neighbors = neighborList.getNeighborIdsMutable(i);

    for (int k = 0; k < numNeighbors; ++k) {
      int j = neighbors[k];
      if (j == -1)
        continue;
      bool isJFailed = (eqPSPtr[j] >= criticalEqPS_);

      // 对于邻点 j 同样要求三轴度约束
      if (isJFailed && stressPtr) {
        int idx9_j = j * 9;
        double trace_j =
            stressPtr[idx9_j] + stressPtr[idx9_j + 4] + stressPtr[idx9_j + 8];
        double sigma_m_j = trace_j / 3.0;
        double s11_j = stressPtr[idx9_j] - sigma_m_j;
        double s22_j = stressPtr[idx9_j + 4] - sigma_m_j;
        double s33_j = stressPtr[idx9_j + 8] - sigma_m_j;
        double s12_j = stressPtr[idx9_j + 1];
        double s21_j = stressPtr[idx9_j + 3];
        double s13_j = stressPtr[idx9_j + 2];
        double s31_j = stressPtr[idx9_j + 6];
        double s23_j = stressPtr[idx9_j + 5];
        double s32_j = stressPtr[idx9_j + 7];

        double s_ij_s_ij_j = s11_j * s11_j + s22_j * s22_j + s33_j * s33_j +
                             s12_j * s12_j + s21_j * s21_j + s13_j * s13_j +
                             s31_j * s31_j + s23_j * s23_j + s32_j * s32_j;
        double seq_j = std::sqrt(1.5 * s_ij_s_ij_j);
        double eta_j = (seq_j > 1e-6) ? (sigma_m_j / seq_j) : 0.0;

        // 邻点不仅受压也可剪切断裂
        // if (eta_j <= -0.3333333333) {
        //   isJFailed = false;
        // }
      }

      if (isFailed && isJFailed) {
        neighbors[k] = -1;
        // 【取消牵连判断】不再随手把粒子置为 0，只单纯断键！
        // activeStatusPtr[i] = 0;
        // activeStatusPtr[j] = 0;
      } else {
        activeBonds++;
      }
    }

    // 【后处理统计】统计剩余键比例，并转换为损伤度 (Damage Index)
    double integrity = calculateFractureRatio(i, activeBonds);
    bondIntegrityPtr[i] = integrity;

    // 【阈值处决】计算本粒子的损伤度: damage = 1.0 - integrity
    // 当损伤度超越设定的宏观处决阈值时，方才将其踢出受力积分运算
    double damage = bondIntegrityPtr[i];
    if (damage >= criticalDamage_) {
      activeStatusPtr[i] = 0; // 名正言顺地死亡，彻底化作自由破片
    }
  }
}

} // namespace PDCommon::Fracture
