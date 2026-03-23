// ============================================================================
// PDEngineInitializer.h - PD 引擎初始化组件
// ============================================================================

#ifndef SRC_ENGINE_SOLVERS_PD_PDENGINE_INITIALIZER_H
#define SRC_ENGINE_SOLVERS_PD_PDENGINE_INITIALIZER_H

#include "PDContext.h"
#include "TimeIntegrator.h"
#include "PDKernel.h"
#include <memory>
#include <string>

namespace Src::Engine::Solvers::PD {

class PDEngineInitializer {
public:
  static void InitModel(PDCommon::Core::PDContext &ctx, const std::string &yamlPath);
  static void InitMaterial(PDCommon::Core::PDContext &ctx, const std::string &yamlPath);
  static void InitFields(PDCommon::Core::PDContext &ctx, const std::string &yamlPath);
  static void InitConditions(PDCommon::Core::PDContext &ctx, const std::string &yamlPath);
  static void InitNeighbors(PDCommon::Core::PDContext &ctx, const std::string &yamlPath);
  static void InitSolverComponents(const std::string &yamlPath,
                                   std::unique_ptr<Src::Integration::TimeIntegrator> &integrator,
                                   std::unique_ptr<PDCommon::Kernel::PDKernel> &kernel,
                                   Src::Integration::SolverConfig &solverConfig);
};

} // namespace Src::Engine::Solvers::PD

#endif // SRC_ENGINE_SOLVERS_PD_PDENGINE_INITIALIZER_H
