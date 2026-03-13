#include "GRPD.h"
#include "SolverEngine.h"
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
  GRPD::Engine::SolverEngine pdtest;
  pdtest.Initialize();

  // pdtest.Solve();

  pdtest.Output();

  system("pause");

  return 0;
}
