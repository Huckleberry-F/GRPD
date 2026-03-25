// ============================================================================
// PDEngine.cpp - PD 求解引擎 (整合自 Initialize, Solve, Output)
// ============================================================================

#include "PDEngine.h"
#include "PDEngineInitializer.h"
#include "BCRegistry.h"
#include "EngineRegistry.h"
#include "FieldManager.h"
#include "FieldRegistry.h"
#include "IOManager.h"
#include "KernelRegistry.h"
#include "Logger.h"
#include "MaterialRegistry.h"
#include "TimeIntegratorRegistry.h"
#include "Outputer.h"
#include "PhysicsFieldRegistry.h"
#include <string>
#include <vector>
#include <yaml-cpp/yaml.h>

namespace Src::Engine::Solvers::PD {

// ---------------------------------------------------------------------------
// 辅助：将注册表类型列表拼接为逗号分隔的字符串
// ---------------------------------------------------------------------------
static std::string joinTypes(const std::vector<std::string> &types) {
  std::string result;
  for (const auto &t : types) {
    result += t + ", ";
  }
  if (!result.empty()) {
    result.erase(result.size() - 2);
  }
  return result;
}

PDEngine::PDEngine() { 
  LOG_INFO("[PDEngine] PD Engine instance created."); 
}

// ---------------------------------------------------------------------------
// printRegistrySummary: 打印 PD 引擎使用的所有注册表信息
// ---------------------------------------------------------------------------
void PDEngine::printRegistrySummary() const {
  LOG_INFO("========== Registered Types Summary ===========");
  LOG_INFO("  TimeIntegrator    : " +
           joinTypes(Src::Integration::TimeIntegratorRegistry::getInstance()
                         .getRegisteredTypes()));
  LOG_INFO("  FieldRegistry     : " +
           joinTypes(PDCommon::Field::FieldRegistry::getInstance().getRegisteredTypes()));
  LOG_INFO("  PhysicsFields     : " +
           joinTypes(PDCommon::Field::PhysicsFieldRegistry::getInstance().getRegisteredTypes()));
  LOG_INFO("  BCRegistry        : " +
           joinTypes(PDCommon::BC::BCRegistry::getInstance().getRegisteredTypes()));
  LOG_INFO("  MaterialRegistry  : " +
           joinTypes(PDCommon::Material::MaterialRegistry::getInstance().getRegisteredTypes()));
  LOG_INFO("  KernelRegistry    : " +
           joinTypes(PDCommon::Kernel::KernelRegistry::getInstance().getRegisteredTypes()));
  LOG_INFO("  EngineRegistry    : " +
           joinTypes(Src::Engine::EngineRegistry::getInstance().getRegisteredTypes()));
  LOG_INFO("===============================================");
}

void PDEngine::Initialize(const std::string &yamlPath) {
  LOG_INFO("[PDEngine] Initializing PD simulation context...");
  yamlPath_ = yamlPath;

  YAML::Node config = YAML::LoadFile(yamlPath_);

  PDEngineInitializer::InitModel(pdContext_, config, yamlPath_);
  PDEngineInitializer::InitMaterial(pdContext_, config);
  PDEngineInitializer::InitFields(pdContext_, config);
  PDEngineInitializer::InitConditions(pdContext_, config);
  PDEngineInitializer::InitNeighbors(pdContext_, config);
  PDEngineInitializer::InitSolverComponents(config, integrator_, kernel_);

  LOG_INFO("[PDEngine] PD simulation context initialized successfully.");
}

void PDEngine::Solve() {
  LOG_INFO("[PDEngine] Starting PD Engine Solve Phase...");

  if (!integrator_ || !kernel_) {
    LOG_ERROR("[PDEngine] Solver components not initialized! "
              "Call Initialize() before Solve().");
    return;
  }

  auto outputCallback = [this](int step, double time) { this->Output(step, time); };

  integrator_->run(pdContext_, *kernel_, outputCallback);

  LOG_INFO("[PDEngine] PD Engine Solve Phase Finished.");
}

void PDEngine::Output() {
  Output(-1, 0.0);
}

void PDEngine::Output(int step, double physicalTime) {
  const auto &currentModel = pdContext_;
  LOG_INFO("Starting data export process step=" + std::to_string(step) + 
           " t=" + std::to_string(physicalTime));

  std::string currentModelName = currentModel.getName();
  bool binaryRequested = false;
  std::string customWriterName;
  std::vector<std::pair<std::string, int>> variablesToOutput;

  try {
    YAML::Node config = YAML::LoadFile(yamlPath_);

    if (config["Writer"]) {
      if (config["Writer"]["Name"]) {
        customWriterName = config["Writer"]["Name"].as<std::string>();
      }
      if (config["Writer"]["Type"]) {
        std::string typeStr = config["Writer"]["Type"].as<std::string>();
        if (typeStr == "BINARY" || typeStr == "binary") {
          binaryRequested = true;
        } else if (typeStr == "ASCII" || typeStr == "ascii") {
          binaryRequested = false;
        } else {
          LOG_INFO("Unknown Writer Type: " + typeStr +
                   ". Using default ASCII.");
        }
      }
      if (config["Writer"]["Variables"]) {
        for (const auto &it : config["Writer"]["Variables"]) {
          std::string varName = it["Name"].as<std::string>();
          int varDim = it["Dimension"] ? it["Dimension"].as<int>() : 1;
          variablesToOutput.push_back({varName, varDim});
        }
      } else {
        variablesToOutput.push_back({"Coords", 3});
        variablesToOutput.push_back({"PartID", 1});
        variablesToOutput.push_back({"Volume", 1});
      }
    }
  } catch (const YAML::Exception &e) {
    LOG_ERROR("Failed to parse YAML file in Output(): " +
              std::string(e.what()));
    return;
  }

  LOG_INFO("Fetching model context for export: " + currentModelName);

  try {
    const auto &fieldManager = currentModel.getFieldManager();
    const auto *coordsField = fieldManager.getFieldAs<double>("Coords");
    if (!coordsField) {
      LOG_ERROR("Field 'Coords' not found. Aborting write.");
      return;
    }
    if (coordsField->getDim() != 3) {
      LOG_ERROR("Field 'Coords' must have dimension 3. Aborting write.");
      return;
    }

    std::string finalOutputName =
        customWriterName.empty() ? currentModelName : customWriterName;
        
    // 通过 IOManager 生成 VTK 输出路径（自动放入 Result_时间戳 文件夹）
    auto &ioManager = PDCommon::IO::IOManager::getInstance();
    std::string vtkOutputPath;
    if (step >= 0) {
      vtkOutputPath = ioManager.buildVtkPath(finalOutputName, step, physicalTime);
    } else {
      vtkOutputPath = ioManager.buildResultPath(finalOutputName + "_output.vtk");
    }
    LOG_INFO("Exporting to VTK format: " + vtkOutputPath);

    const size_t numParticles = coordsField->size();
    if (numParticles == 0) {
      LOG_INFO("No particles to export for model: " + currentModelName);
      return;
    }

    PDCommon::IO::Outputer outputer;
    if (binaryRequested) {
      outputer.setFormat(PDCommon::IO::binary);
      LOG_INFO("Output format set to BINARY.");
    } else {
      LOG_INFO("Output format set to ASCII.");
    }

    for (const auto &[varName, yamlDim] : variablesToOutput) {
      if (varName == "Coords") {
        continue;
      }

      if (!fieldManager.hasField(varName)) {
        LOG_WARNING("Unknown output variable requested: " + varName +
                    ". Skipping.");
        continue;
      }

      if (varName == "ID" || varName == "PartID" || varName == "MatID") {
        outputer.addIntField(varName);
        continue;
      }

      const auto *field = fieldManager.getField(varName);
      if (!field) {
        LOG_WARNING("Field '" + varName + "' is null. Skipping.");
        continue;
      }

      const int fieldDim = field->getDim();
      if (fieldDim == 3 || yamlDim == 3) {
        outputer.addVectorField(varName);
      } else if (fieldDim == 1) {
        outputer.addScalarField(varName);
      } else {
        LOG_WARNING("Field '" + varName + "' has unsupported dimension " +
                    std::to_string(fieldDim) + ". Skipping.");
      }
    }

    if (!outputer.writeVTK(vtkOutputPath, fieldManager, numParticles)) {
      LOG_ERROR("VTK export failed for model: " + currentModelName);
      return;
    }

    LOG_INFO("Successfully exported data for model: " + currentModelName);
  } catch (const std::exception &e) {
    LOG_ERROR("Write process aborted: " + std::string(e.what()));
  }
}

} // namespace Src::Engine::Solvers::PD

REGISTER_ENGINE_TYPE(PD, []() {
  return std::make_unique<Src::Engine::Solvers::PD::PDEngine>();
});
