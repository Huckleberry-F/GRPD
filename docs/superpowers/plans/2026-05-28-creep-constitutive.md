# Creep Constitutive Models Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [x]`) syntax for tracking.

**Goal:** 实现三个蠕变本构模型（Norton-Bailey、时间硬化、应变硬化），并安全扩展 `PDContext` 使得材料对象能获取时间步长和绝对物理时间。

**Architecture:** 采用策略模式（Strategy Pattern）。首先扩展 `PDContext` 和 `MechanicalMaterial` 接口支持时间戳传递。接着编写统一的 `CreepMaterialBase` 基类处理复杂的 3D 张量径向返回算法及状态变量（等效蠕变应变与蠕变应变张量）。最后通过轻量级的派生类实现三种特定的 1D 标量蠕变经验律。

**Tech Stack:** C++17, Eigen3, OpenMP, GTest

---

### Task 1: Update PDContext Time Tracking

**Files:**
- Modify: `PDCommon/Core/include/PDContext.h`
- Modify: `PDCommon/Core/src/PDContext.cpp` (if needed for definition)

- [x] **Step 1: Write the failing test**
  *(跳过本步由于无状态类无直接 gtest，通过编译检测即可)*
- [x] **Step 2: Write minimal implementation**
  修改 `PDContext.h` 增加 `double currentTime_ = 0.0;`
  ```cpp
  // 在类中新增
  double getCurrentTime() const { return currentTime_; }
  void setCurrentTime(double t) { currentTime_ = t; }
  ```
- [x] **Step 3: Commit**
  ```bash
  git add PDCommon/Core/include/PDContext.h
  git commit -m "core: add currentTime tracking to PDContext"
  ```

### Task 2: Update Integrators

**Files:**
- Modify: `Src/Integration/src/TimeIntegrator.cpp`
- Modify: `Src/Integration/src/ExplicitEuler.cpp`
- Modify: `Src/Integration/src/CentralDifference.cpp`
- Modify: `Src/Integration/src/ADR_Integrator.cpp`

- [x] **Step 1: Write minimal implementation**
  在所有 Integrator 循环中，找到当前积分时间变量（如 `currentTime`），并在调用 `evaluateForces` 之前插入：
  ```cpp
  ctx.setCurrentTime(currentTime);
  ```
- [x] **Step 2: Run test to verify it passes**
  运行现有的单元测试确保无崩溃：
  ```bash
  cd build && ctest -R UnitTest
  ```
- [x] **Step 3: Commit**
  ```bash
  git add Src/Integration/src/*.cpp
  git commit -m "integration: sync currentTime to PDContext"
  ```

### Task 3: Update Material Interface

**Files:**
- Modify: `PDCommon/Material/include/MechanicalMaterial.h`
- Modify: `PDCommon/Kernel/src/NOSB_M.cpp`
- Modify: `PDCommon/Material/include/LinearElasticMat.h`, `J2PlasticityMat.h`, `JCPlasticityMat.h` (及相关的 .cpp)

- [x] **Step 1: Write minimal implementation**
  修改 `MechanicalMaterial.h`：
  ```cpp
  virtual Eigen::Matrix3d ComputePK1Stress(const Eigen::Matrix3d &F, int particleId = -1, int stateMode = 0, const PDCommon::Core::PDContext* ctx = nullptr) const = 0;
  ```
  同步修改所有子类的方法签名。
  修改 `NOSB_M.cpp`：
  ```cpp
  Eigen::Matrix3d P_mat = matArrCache_[i]->ComputePK1Stress(F_mat, effectiveId, stateMode, &ctx);
  ```
- [x] **Step 2: Run test to verify it passes**
  ```bash
  cd build && make -j12
  ```
- [x] **Step 3: Commit**
  ```bash
  git add PDCommon/Material/ PDCommon/Kernel/
  git commit -m "material: extend ComputePK1Stress to accept PDContext"
  ```

### Task 4: Create CreepMaterialBase

**Files:**
- Create: `PDCommon/Material/include/CreepMaterialBase.h`
- Create: `PDCommon/Material/src/CreepMaterialBase.cpp`
- Modify: `PDCommon/Material/CMakeLists.txt`

- [x] **Step 1: Write minimal implementation**
  分配状态变量并实现 Radial Return。
  *(由于代码较长，将在实际执行阶段提供完整 CPP)*
  核心：
  ```cpp
  virtual double computeDeltaCreepStrain(double q_trial, double dt, double t, double eq_cr_old) const = 0;
  ```
- [x] **Step 2: Update CMakeLists**
  添加 `src/CreepMaterialBase.cpp`
- [x] **Step 3: Commit**
  ```bash
  git add PDCommon/Material/
  git commit -m "material: add CreepMaterialBase with radial return mapping"
  ```

### Task 5: Create Specific Creep Models

**Files:**
- Create: `PDCommon/Material/include/NortonBaileyCreepMat.h` & `.cpp`
- Create: `PDCommon/Material/include/TimeHardeningCreepMat.h` & `.cpp`
- Create: `PDCommon/Material/include/StrainHardeningCreepMat.h` & `.cpp`

- [x] **Step 1: Write minimal implementation**
  例如 NortonBailey:
  ```cpp
  double computeDeltaCreepStrain(double q_trial, double dt, double t, double eq_cr_old) const override {
      return A_ * std::pow(q_trial, n_) * dt;
  }
  ```
- [x] **Step 2: Run test to verify it passes**
  ```bash
  cd build && make -j12
  ```
- [x] **Step 3: Commit**
  ```bash
  git add PDCommon/Material/
  git commit -m "material: implement NortonBailey, Time and Strain hardening models"
  ```


