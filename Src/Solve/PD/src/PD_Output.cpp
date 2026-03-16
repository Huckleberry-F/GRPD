// ============================================================================
// PD_Output.cpp - PD 计算结果输出（从 SE_Output.cpp 迁移）
// ============================================================================

#include "PDSolver.h"
#include "FieldManager.h"
#include "Logger.h"
#include "Outputer.h"
#include <string>
#include <vector>
#include <yaml-cpp/yaml.h>

namespace Src::Solve {

void PDSolver::Output() {
  const auto &currentModel = pdContext_;
  LOG_INFO("Starting data export process...");

  std::string currentModelName = currentModel.getName();
  bool binaryRequested = false;
  std::string customWriterName;
  std::vector<std::pair<std::string, int>> variablesToOutput;

  try {
    YAML::Node config = YAML::LoadFile(yamlPath_);

    if (!(config["Assembly"] && config["Assembly"]["OutputGrpd"])) {
      LOG_ERROR("Key [Assembly][OutputGrpd] not found in YAML file!");
      return;
    }

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
    std::string vtkOutputPath =
        "../../Output/" + finalOutputName + "_output.vtk";
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

} // namespace Src::Solve
