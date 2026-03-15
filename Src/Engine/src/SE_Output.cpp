// ============================================================================
// SE_Output.cpp - Output module
// Responsibility: Extract the correct ParticleManager context and export
//                 simulation data to files (e.g. VTK).
// ============================================================================
#include "FieldManager.h"
#include "Logger.h"
#include "PDSimulater.h"
#include "SolverEngine.h"
#include "TypedField.h"
#include "VtkWriter.h"
#include <string>
#include <vector>
#include <yaml-cpp/yaml.h>


namespace GRPD::Engine {

void SolverEngine::OutputPD() {
  const auto &currentModel = this->pdContext_;
  LOG_INFO("Starting data export process...");

  // =================================================================
  // 1. Parse YAML to find the current model context name
  // =================================================================
  std::string yamlPath = "../../Input/PD.yaml";
  std::string grpdPath = "";
  std::string currentModelName = currentModel.getName();
  GRPD::IO::fileFormat format = GRPD::IO::ascii; // default to ascii
  std::string customWriterName = "";
  std::vector<std::pair<std::string, int>>
      variablesToOutput; // {Name, Dimension}

  try {
    YAML::Node config = YAML::LoadFile(yamlPath);

    if (config["Assembly"] && config["Assembly"]["OutputGrpd"]) {
      // We already have the model name from the passed object
    } else {
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
          format = GRPD::IO::binary;
        } else if (typeStr == "ASCII" || typeStr == "ascii") {
          format = GRPD::IO::ascii;
        } else {
          LOG_INFO("Unknown Writer Type: " + typeStr +
                   ". Using default ASCII.");
        }
      }
      if (config["Writer"]["Variables"]) {
        for (auto it : config["Writer"]["Variables"]) {
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
    LOG_ERROR("Failed to parse YAML file in Write(): " + std::string(e.what()));
    return;
  }

  // =================================================================
  // 2. Fetch the corresponding ParticleManager from model context
  // =================================================================
  LOG_INFO("Fetching model context for export: " + currentModelName);

  try {
    const auto &manager = currentModel.getParticleManager();
    const auto &fieldManager = currentModel.getFieldManager();

    // =================================================================
    // 3. Export data using VtkWriter
    // =================================================================
    std::string finalOutputName =
        customWriterName.empty() ? currentModelName : customWriterName;
    std::string vtkOutputPath =
        "../../Output/" + finalOutputName + "_output.vtk";
    LOG_INFO("Exporting to VTK format: " + vtkOutputPath);

    int numParticles = static_cast<int>(manager.getTotalParticles());

    if (numParticles == 0) {
      LOG_INFO("No particles to export for model: " + currentModelName);
      return;
    }

    GRPD::IO::VtkWriter writer(vtkOutputPath, numParticles, format);

    // Set points geometry
    double *coordsPtr = manager.getGeomDataPtr(GRPD::Model::ModelVar::Coords);
    if (coordsPtr) {
      writer.setPointsInfo(GRPD::IO::Float64, coordsPtr, "Coordinates", 3);
    } else {
      LOG_ERROR("Failed to extract Coordinates. Aborting write.");
      return;
    }

    // Set physical variables dynamically based on user config
    for (const auto &[varName, yamlDim] : variablesToOutput) {
      if (varName == "Coords") {
        continue; // Coordinates are handled separately above
      }

      // ---------------------------------------------------------------
      // 优先从 FieldManager 查询物理场变量
      // FieldManager 统一存储所有物理场（Temperature, HeatFlux 等）
      // ---------------------------------------------------------------
      if (fieldManager.hasField(varName)) {
        const auto *field = fieldManager.getField(varName);
        if (field && field->rawPtr()) {
          writer.setPointsVariable(GRPD::IO::Float64,
                                   const_cast<double *>(field->rawPtr()),
                                   varName.c_str(), yamlDim);
          LOG_INFO("Exported field '" + varName + "' from FieldManager.");
        } else {
          LOG_WARNING("Field '" + varName +
                      "' data pointer is null. Skipping.");
        }
        continue;
      }

      // ---------------------------------------------------------------
      // 通用路由：来自 ParticleManager 的几何/拓扑变量
      // ---------------------------------------------------------------
      GRPD::Model::ModelVar enumName;
      GRPD::Model::ParticleManager::VarDataType dataType;
      int managerDim = 1;

      if (GRPD::Model::ParticleManager::getModelVarInfo(varName, enumName,
                                                        dataType, managerDim)) {
        void *dataPtr = nullptr;
        GRPD::IO::dataType vtkType = GRPD::IO::Float64;

        // Fetch the corresponding data pointer based on its specific type
        if (dataType == GRPD::Model::ParticleManager::VarDataType::Double) {
          dataPtr = manager.getGeomDataPtr(enumName);
          vtkType = GRPD::IO::Float64;
        } else if (dataType == GRPD::Model::ParticleManager::VarDataType::Int) {
          dataPtr = manager.getIntDataPtr(enumName);
          vtkType = GRPD::IO::Int32;
        }

        // Use the dimension specified in YAML configuration
        int finalDim = yamlDim;

        // If pointer extraction was successful, pass it to VTK Writer
        if (dataPtr) {
          writer.setPointsVariable(vtkType, dataPtr, varName.c_str(), finalDim);
        } else {
          LOG_WARNING("Data array for " + varName +
                      " is empty or null. Skipping.");
        }
      } else {
        LOG_WARNING("Unknown output variable requested: " + varName +
                    ". Skipping.");
      }
    }
    // =================================================================
    // 4. Execute writing
    // =================================================================
    writer.write();

    LOG_INFO("Successfully exported data for model: " + currentModelName);

  } catch (const std::exception &e) {
    LOG_ERROR("Write process aborted: " + std::string(e.what()));
  }
}

} // namespace GRPD::Engine
