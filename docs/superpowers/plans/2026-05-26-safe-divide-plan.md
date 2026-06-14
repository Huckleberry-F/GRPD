# Safe Divide Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement safeDivide utility function and test target to guarantee it passes numerical constraints.

**Architecture:** Header-only inline function in PDCommon/Utils, tested via a standalone executable.

**Tech Stack:** C++17, CMake

---

### Task 1: CMake Setup and Test Stub

**Files:**
- Create: [test_math_utils.cpp](file:///d:/Project_C++/GRPD/Src/test_math_utils.cpp)
- Modify: [CMakeLists.txt](file:///d:/Project_C++/GRPD/Src/CMakeLists.txt)

- [ ] **Step 1: Create a placeholder main function**
  Write a stub main function in `Src/test_math_utils.cpp` that simply prints a startup log and returns 0.
  
  ```cpp
  #include <iostream>
  int main() {
      std::cout << "[test_math_utils] Starting test stub..." << std::endl;
      return 0;
  }
  ```
- [ ] **Step 2: Modify CMakeLists.txt to register test_math_utils target**
  Register the executable target in `Src/CMakeLists.txt` and link it to PDCommon.
  
  ```cmake
  # 5. Add test_math_utils target
  add_executable(test_math_utils test_math_utils.cpp)
  target_include_directories(test_math_utils PRIVATE
      ${PROJECT_SOURCE_DIR}/PDCommon/Utils/include
      ${PROJECT_SOURCE_DIR}/PDCommon/Material/include
      ${PROJECT_SOURCE_DIR}/PDCommon/BC/include
      ${PROJECT_SOURCE_DIR}/PDCommon/Core/include
      ${PROJECT_SOURCE_DIR}/Src
      ${PROJECT_SOURCE_DIR}/Src/Engine/include
  )
  target_link_libraries(test_math_utils PRIVATE
      PDCommon
      Engine_Obj
      PDSolve_Obj
      Material_Obj
      BC_Obj
      IO_Obj
      Model_Obj
      Neighbor_Obj
      Core_Obj
      Field_Obj
      Kernel_Obj
      Integration_Obj
      Fracture_Obj
      Contact_Obj
      PostProcessing_Obj
  )
  ```
- [ ] **Step 3: Build the target to verify CMake setup works**
  Run CMake configure and build `test_math_utils`.
- [ ] **Step 4: Run the stub executable**
  Verify that the executable runs and prints the startup log successfully.
- [ ] **Step 5: Commit**
  Commit `Src/CMakeLists.txt` and `Src/test_math_utils.cpp`.

### Task 2: Write Failing TDD Tests (RED)

**Files:**
- Modify: [test_math_utils.cpp](file:///d:/Project_C++/GRPD/Src/test_math_utils.cpp)

- [ ] **Step 1: Write TDD assertions that fail**
  Add assertions in `test_math_utils.cpp` to verify `safeDivide` behavior. Because `MathUtils.h` does not exist yet, this will fail to compile.
  
  ```cpp
  #include "MathUtils.h"
  #include <iostream>
  #include <cassert>
  #include <cmath>

  int main() {
      std::cout << "[test_math_utils] Starting TDD test..." << std::endl;
      
      // Test 1: Normal division
      assert(std::abs(PDCommon::Utils::safeDivide(10.0, 2.0) - 5.0) < 1e-15);
      
      // Test 2: Exactly zero division (using default fallback = 0.0)
      assert(PDCommon::Utils::safeDivide(10.0, 0.0) == 0.0);
      
      // Test 3: Exactly zero division (using custom fallback = 99.0)
      assert(PDCommon::Utils::safeDivide(10.0, 0.0, 99.0) == 99.0);
      
      // Test 4: Epsilon trigger (denominator absolute value < epsilon)
      assert(PDCommon::Utils::safeDivide(10.0, 1e-16, 99.0, 1e-15) == 99.0);
      
      // Test 5: Safe execution just above epsilon
      assert(std::abs(PDCommon::Utils::safeDivide(10.0, 1e-14, 99.0, 1e-15) - 1e15) < 1e-15);
      
      std::cout << "[test_math_utils] All tests PASSED!" << std::endl;
      return 0;
  }
  ```
- [ ] **Step 2: Try to build the target and observe compile failure**
  Build the target. Expected: Compile fails because `MathUtils.h` is missing.
- [ ] **Step 3: Create an empty header to resolve include error but fail tests**
  Create an empty header `PDCommon/Utils/include/MathUtils.h` with just namespace and empty function that returns a dummy value (e.g., always 0.0).
  
  ```cpp
  #ifndef PDCOMMON_UTILS_MATH_UTILS_H
  #define PDCOMMON_UTILS_MATH_UTILS_H
  namespace PDCommon::Utils {
  inline double safeDivide(double numerator, double denominator, double fallback = 0.0, double epsilon = 1e-15) {
      return 0.0;
  }
  }
  #endif
  ```
- [ ] **Step 4: Build and run the test to see assertions fail**
  Build the target. Run the test. Expected: Test crashes/fails on assertions (e.g. `safeDivide(10.0, 2.0)` expects 5.0 but gets 0.0).
- [ ] **Step 5: Commit**
  Commit the failing tests and the empty stub header.

### Task 3: Implement safeDivide (GREEN)

**Files:**
- Modify: [MathUtils.h](file:///d:/Project_C++/GRPD/PDCommon/Utils/include/MathUtils.h)

- [ ] **Step 1: Write the minimal implementation to satisfy assertions**
  Implement the epsilon-based comparison check inside `safeDivide`.
  
  ```cpp
  #ifndef PDCOMMON_UTILS_MATH_UTILS_H
  #define PDCOMMON_UTILS_MATH_UTILS_H
  #include <cmath>
  namespace PDCommon::Utils {
  inline double safeDivide(double numerator, double denominator, double fallback = 0.0, double epsilon = 1e-15) {
      if (std::abs(denominator) < epsilon) {
          return fallback;
      }
      return numerator / denominator;
  }
  }
  #endif
  ```
- [ ] **Step 2: Build and run the test target**
  Verify that compiling succeeds and all assertions pass.
- [ ] **Step 3: Commit**
  Commit the working header.
