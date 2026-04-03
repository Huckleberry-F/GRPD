#include "PDEnginePre.h"
#include "Logger.h"
#include "FieldManager.h"
#include "FieldRegistry.h"
#include "Material.h"
#include "MaterialManager.h"
#include "MeshData.h"
#include "ParticleManager.h"
#include "PhysicsFieldRegistry.h"
#include "PhysicsFields.h"
#include <string>

namespace Src::Engine::Solvers::PD::Init {

// ============================================================================
// 步骤 3：初始化引擎字段空间和状态池
// 通过 PhysicsFieldRegistry 识别待求解字段，再从所有已注册的独立 Materials 
// 获取状态变量长度来扩展 SDV (State Dependent Variables) 池。
// ============================================================================
void InitFields(PDCommon::Core::PDContext &ctx,
                const YAML::Node &config) {
  LOG_INFO("[InitFields] ==================================================");
  LOG_INFO("[InitFields] Entering Field Registration Phase...");
  LOG_INFO("[InitFields] ==================================================");

  std::string solverType = "";
  if (config["Solver"] && config["Solver"]["Type"]) {
    solverType = config["Solver"]["Type"].as<std::string>();
  }

  auto &fieldManager = ctx.getFieldManager();

  auto &registry = PDCommon::Field::PhysicsFieldRegistry::getInstance();
  bool anyMatch = false;

  for (const auto &registeredType : registry.getRegisteredTypes()) {
    if (solverType.find(registeredType) != std::string::npos) {
      auto fields = registry.create(registeredType);
      if (fields) {
        fields->registerFields(fieldManager);
        anyMatch = true;
        LOG_INFO("[InitFields] Auto-registered PhysicsFields module: " +
                 registeredType);
      }
    }
  }

  if (!anyMatch) {
    LOG_WARNING("[InitFields] Solver type '" + solverType +
                "' did not match any registered PhysicsFields types.");
  }

  PDCommon::IO::MeshData::ensureParticleFields(fieldManager);

  auto &matManager = ctx.getMaterialManager();
  size_t maxSDVs = 0;
  for (const auto &[matName, matPtr] : matManager.getMaterials()) {
    if (matPtr) {
      maxSDVs = std::max(maxSDVs, matPtr->getNumStateVariables());
    }
  }

  if (maxSDVs > 0) {
    auto sdvField = PDCommon::Field::FieldRegistry::getInstance()
        .createField("DoubleField", "SDV", static_cast<int>(maxSDVs));
    fieldManager.addField(std::move(sdvField));
    LOG_INFO("[InitFields] SDV Pool registered with dimension: " +
             std::to_string(maxSDVs));
  }

  for (const auto &[matName, matPtr] : matManager.getMaterials()) {
    if (matPtr) {
      matPtr->allocateStateVariables(fieldManager);
    }
  }

  size_t numParticles = ctx.getParticleManager().getTotalParticles();
  fieldManager.resizeAll(numParticles);

  // 在全部场分配之后进行指针的高速挂载
  LOG_INFO("[InitFields] Binding high-speed material state pointers...");
  for (const auto &[matName, matPtr] : matManager.getMaterials()) {
    if (matPtr) {
      matPtr->bindStateVariables(fieldManager);
    }
  }

  if (!PDCommon::IO::MeshData::populateParticleFields(
          ctx.getParticleManager(), fieldManager)) {
    LOG_ERROR("[InitFields] Failed to populate particle fields from "
              "ParticleManager.");
    exit(EXIT_FAILURE);
  }

  LOG_INFO("[InitFields] All fields allocated for " +
           std::to_string(numParticles) + " particles. Total fields: " +
           std::to_string(fieldManager.getFieldCount()) + ".");
}

} // namespace Src::Engine::Solvers::PD::Init
