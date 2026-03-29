#include "PDEnginePre.h"
#include "MaterialManager.h"
#include "Material.h"
#include "DamageModel.h"
#include "Logger.h"

namespace Src::Engine::Solvers::PD::Init {

// ============================================================================
// 独立初始化步骤：初始化带有专属 DamageModel 的材料（需在近邻表构建之后进行）
// ============================================================================
void InitDamageModels(PDCommon::Core::PDContext &ctx, const YAML::Node &config) {
  LOG_INFO("Initializing Material-specific Damage Models...");

  auto &matManager = ctx.getMaterialManager();
  int damageModelsCount = 0;

  for (const auto &[name, mat] : matManager.getMaterials()) {
      if (mat && mat->getDamageModel()) {
          mat->getDamageModel()->preCompute(ctx, mat->getMatId());
          damageModelsCount++;
      }
  }

  LOG_INFO("Material Damage Models initialized. Total Active Models: " + std::to_string(damageModelsCount));
}

} // namespace Src::Engine::Solvers::PD::Init
