#include "IOManager.h"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace fs = std::filesystem;

int main() {
  const fs::path oldCwd = fs::current_path();
  const fs::path testDir =
      fs::temp_directory_path() / "grpd_io_manager_vtk_path_test";

  fs::remove_all(testDir);
  fs::create_directories(testDir);

  {
    std::ofstream yaml(testDir / "PD.yaml");
    yaml << "Solver:\n";
    yaml << "  Engine: PD\n";
  }

  fs::current_path(testDir);

  auto &ioManager = PDCommon::IO::IOManager::getInstance();
  ioManager.initialize();

  const std::string vtkPath = ioManager.buildVtkPath("Beam", 42, 0.125);
  const std::string filename = fs::path(vtkPath).filename().string();

  fs::current_path(oldCwd);

  if (filename != "Beam_t0.1250.vtk") {
    std::cerr << "Unexpected VTK filename: " << filename << std::endl;
    return EXIT_FAILURE;
  }

  if (filename.find("_step") != std::string::npos) {
    std::cerr << "VTK filename still contains step suffix: " << filename
              << std::endl;
    return EXIT_FAILURE;
  }

  // Check .vtk.series file (JSON format for Legacy VTK time series)
  fs::path seriesPath = ioManager.getResultDir() / "Beam.vtk.series";
  if (!fs::exists(seriesPath)) {
    std::cerr << ".vtk.series file not created: " << seriesPath << std::endl;
    return EXIT_FAILURE;
  }

  std::ifstream seriesFile(seriesPath);
  std::string seriesContent((std::istreambuf_iterator<char>(seriesFile)),
                            std::istreambuf_iterator<char>());
  if (seriesContent.find("\"file-series-version\"") == std::string::npos ||
      seriesContent.find("Beam_t0.1250.vtk") == std::string::npos ||
      seriesContent.find("0.125") == std::string::npos) {
    std::cerr << ".vtk.series file content mismatch:\n"
              << seriesContent << std::endl;
    return EXIT_FAILURE;
  }

  std::cout << "[OK] VTK filename uses time-only suffix: " << filename
            << " and .vtk.series file created successfully." << std::endl;
  return EXIT_SUCCESS;
}
