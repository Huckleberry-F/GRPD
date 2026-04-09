#include "PDEnginePre.h"
#include "MaterialManager.h"
#include "Material.h"
#include "FractureModel.h"
#include "Logger.h"

namespace Src::Engine::Solvers::PD::Init {

// ============================================================================
// 独立初始化步骤：初始化带有专属 FractureModel 的材料（需在近邻表构建之后进行）
// ============================================================================
void InitFractureModels(PDCommon::Core::PDContext &ctx, const YAML::Node &config) {
  LOG_INFO("Initializing Material-specific BondIntegrity Models...");

  auto &matManager = ctx.getMaterialManager();
  int bondIntegrityModelsCount = 0;

  for (const auto &[name, mat] : matManager.getMaterials()) {
      if (mat && mat->getFractureModel()) {
          mat->getFractureModel()->preCompute(ctx, mat->getMatId());
          bondIntegrityModelsCount++;
      }
  }

  LOG_INFO("Material BondIntegrity Models initialized. Total Active Models: " + std::to_string(bondIntegrityModelsCount));
}

} // namespace Src::Engine::Solvers::PD::Init
