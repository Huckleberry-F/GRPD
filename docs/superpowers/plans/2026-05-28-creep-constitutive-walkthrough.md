# Walkthrough: Creep Constitutive Model Implementation

## What was accomplished

1.  **Time Management Infrastructure**:
    *   Added `currentTime_` and `currentDt_` attributes and accessors to `PDContext`.
    *   Updated time integrators (`ExplicitEuler`, `CentralDifference`, `ADR_Integrator`) to correctly compute and sync current step time into `PDContext`.
    *   This infrastructure ensures that all materials downstream have access to global simulation time natively.

2.  **Interface Extension**:
    *   Extended `MechanicalMaterial::ComputePK1Stress` and `ComputeEngineeringStress` to accept `const PDContext* ctx = nullptr`.
    *   Updated all derived physical materials (`LinearElasticMat`, `J2PlasticityMat`, `JCPlasticityMat`, `J2VoceLemaitreMat`) to match this signature.
    *   Updated kernel solvers (`NOSB_M`, `NOSB_Axis`) and tests (`test_constitutive`) to forward the `PDContext` to the material routines.

3.  **Creep Material Base Class (`CreepMaterialBase`)**:
    *   Created an abstract base class inheriting from `MechanicalMaterial`.
    *   Allocated state variables `EqCreepStrain` (scalar) and `CreepStrain` (tensor) along with their Old/Trial copies.
    *   Implemented 3D Radial Return Mapping using Prandtl-Reuss flow rules inside `ComputePK1Stress`.
    *   Created a pure virtual `computeDeltaCreepStrain()` to serve as the unified API for empirical 1D creep equations.

4.  **Derived Empirical Models**:
    *   Implemented **Norton-Bailey Creep** (`NortonBaileyCreepMat`): $\dot{\varepsilon}_{cr} = A q^n$
    *   Implemented **Time Hardening Creep** (`TimeHardeningCreepMat`): $\dot{\varepsilon}_{cr} = A q^n t^m$
    *   Implemented **Strain Hardening Creep** (`StrainHardeningCreepMat`): $\dot{\varepsilon}_{cr} = A q^n (\varepsilon_{cr})^m$
    *   All models dynamically register to `MaterialRegistry` and parse parameters securely.

5.  **Build System & Verification**:
    *   Resolved complex circular `#include` dependencies (`PDContext` vs `MaterialManager`) by modifying `PDCommon/Material/CMakeLists.txt` include directories.
    *   Validated memory layouts using SoA `TypedField` types.
    *   Successfully compiled and linked `libPDCommon.a` and `test_constitutive.exe`.
