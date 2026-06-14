# Safe Divide Math Utility Design

## Overview
In peridynamic computations and mechanical constitutive model implementations, physics loops frequently perform division operations (e.g., inverting NOSB-PD tensors, calculating damage variables, or normalizing force terms). If a denominator approaches zero or is exactly zero, standard division leads to `NaN` or infinity, resulting in solver convergence failure or simulation crash.

This design introduces a high-performance, header-only inline math utility `safeDivide` in the `PDCommon::Utils` namespace to capture and handle zero-division events safely.

## Triggering Context
Use this utility in peridynamic force kernels, constitutive integration loops, or solver updates where division by variable quantities could yield zero values.

## Architectural Approach
Implement a Header-Only inline helper function inside [MathUtils.h](file:///d:/Project_C++/GRPD/PDCommon/Utils/include/MathUtils.h) under namespace `PDCommon::Utils`.
- **Signature**:
  ```cpp
  inline double safeDivide(double numerator, double denominator, double fallback = 0.0, double epsilon = 1e-15);
  ```
- **Zero-Division Condition**:
  $|\text{denominator}| < \epsilon$

## Proposed Changes

### [PDCommon/Utils](file:///d:/Project_C++/GRPD/PDCommon/Utils)

#### [NEW] [MathUtils.h](file:///d:/Project_C++/GRPD/PDCommon/Utils/include/MathUtils.h)
Defines the safe divide function in `PDCommon::Utils` namespace.

### [Src](file:///d:/Project_C++/GRPD/Src)

#### [NEW] [test_math_utils.cpp](file:///d:/Project_C++/GRPD/Src/test_math_utils.cpp)
A minimal, independent test suite checking that:
- Normal division evaluates correctly.
- Division by exactly `0.0` yields the configured fallback value.
- Division by a value smaller than the configured `epsilon` threshold yields the fallback value.
- Division by a value larger than the configured `epsilon` threshold yields the expected mathematical ratio.

#### [MODIFY] [CMakeLists.txt](file:///d:/Project_C++/GRPD/Src/CMakeLists.txt)
Register the new test target `test_math_utils` inside the main build engine.

## Verification Plan

### Automated Tests
Build the project using CMake and run the generated executable:
```bash
./bin/debug/test_math_utils (Windows MSVC Debug)
# or
./bin/release/test_math_utils (MSVC Release)
```
Expected output:
```
[test_math_utils] Starting test...
[test_math_utils] All tests PASSED!
```
