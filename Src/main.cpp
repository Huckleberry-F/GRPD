#include "EngineManager.h"
#include "GRPD.h"
#include <cstdlib>

int main(int argc, char *argv[]) {

  // =================================================================
  // 1. Initialize logger and show Logo
  // =================================================================
  Start();

  // =================================================================
  // 2. Automated pre-processing: invoke Python to generate mesh
  // and read mesh to Particle
  // =================================================================
  Src::Engine::EngineManager GRSIM;
  GRSIM.Initialize();

  GRSIM.Solve();

  GRSIM.Output();

  system("pause");

  return 0;
}
