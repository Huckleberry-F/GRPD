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
#include <yaml-cpp/yaml.h>

namespace Src::Engine::Solvers::PD {

class PDEngineInitializer {
public:
  static void InitModel(PDCommon::Core::PDContext &ctx, const YAML::Node &config, const std::string &yamlPath);
  static void InitMaterial(PDCommon::Core::PDContext &ctx, const YAML::Node &config);
  static void InitFields(PDCommon::Core::PDContext &ctx, const YAML::Node &config);
  static void InitConditions(PDCommon::Core::PDContext &ctx, const YAML::Node &config);
  static void InitNeighbors(PDCommon::Core::PDContext &ctx, const YAML::Node &config);
  static void InitSolverComponents(const YAML::Node &config,
                                   std::unique_ptr<Src::Integration::TimeIntegrator> &integrator,
                                   std::unique_ptr<PDCommon::Kernel::PDKernel> &kernel);
};

} // namespace Src::Engine::Solvers::PD

#endif // SRC_ENGINE_SOLVERS_PD_PDENGINE_INITIALIZER_H
