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

  // Check PVD file
  fs::path pvdPath = ioManager.getResultDir() / "Beam.pvd";
  if (!fs::exists(pvdPath)) {
    std::cerr << "PVD file not created: " << pvdPath << std::endl;
    return EXIT_FAILURE;
  }

  std::ifstream pvdFile(pvdPath);
  std::string pvdContent((std::istreambuf_iterator<char>(pvdFile)),
                         std::istreambuf_iterator<char>());
  if (pvdContent.find("timestep=\"0.125\"") == std::string::npos ||
      pvdContent.find("file=\"Beam_t0.1250.vtk\"") == std::string::npos) {
    std::cerr << "PVD file content mismatch:\n" << pvdContent << std::endl;
    return EXIT_FAILURE;
  }

  std::cout << "[OK] VTK filename uses time-only suffix: " << filename
            << " and PVD file created successfully." << std::endl;
  return EXIT_SUCCESS;
}
