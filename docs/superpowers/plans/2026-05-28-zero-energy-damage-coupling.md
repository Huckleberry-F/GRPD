# Zero Energy Damage Coupling Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 引入粒子标量损伤场 `Damage_Trial` 对 NOSB-PD 零能稳定惩罚力（Silling、Wan、Zhang 稳定器）的耦合软化缩减，并支持通过 YAML 的 `ZeroEnergyDamageCoupling` 参数灵活控制。

**Architecture:** 在 `Stabilizer` 基类中维护 `damageCoupling_` 配置开关，在各机械稳定器的受力计算中提取 `Damage_Trial` 场并将惩罚力缩减为原本的 $\max(0.0, 1.0 - d_i)$ 倍。通过 `NOSB_Base` 统一解析 YAML 参数并传递至稳定器，未显式配置时默认开启耦合且能平滑降级（无损伤场时不报错且不软化）。

**Tech Stack:** C++17, Eigen, CMake, YAML-cpp

---

### Task 1: 稳定器基类接口扩展 (Stabilizer.h)

**Files:**
- Modify: `PDCommon/Kernel/include/Stabilizer.h`

- [x] **Step 1: 编写接口调用校验代码（Failing Compilation Test）**
  - 在 `PDCommon/Kernel/src/StabilizerRegistry.cpp` 临时添加测试行：
    ```cpp
    // 临时测试以确认 damageCoupling 接口尚未定义
    void check_interface(Stabilizer* s) {
        s->setDamageCoupling(true);
    }
    ```
- [x] **Step 2: 运行编译以验证报错**
  - 运行：`cmake --build build --config Release`
  - 预期：编译失败，提示 `class PDCommon::Kernel::Stabilizer has no member named setDamageCoupling`
- [x] **Step 3: 实现成员变量与 Setters**
  - 移除 `StabilizerRegistry.cpp` 中的临时代码。
  - 在 `PDCommon/Kernel/include/Stabilizer.h` 类中新增 `damageCoupling_` 变量（默认为 `true`）以及 `setDamageCoupling(bool enabled)` 接口。
  - 代码改动详情：
    ```cpp
    // 增加的 Setter 接口：
    void setDamageCoupling(bool enabled) { damageCoupling_ = enabled; }
    
    // 增加的成员变量：
    bool damageCoupling_{true};  ///< 是否开启零能模式与粒子损伤的耦合软化，默认开启
    ```
- [x] **Step 4: 重新编译验证通过**
  - 运行：`cmake --build build --config Release`
  - 预期：编译 PASS 成功
- [x] **Step 5: Git Commit**
  - 运行：
    ```bash
    git add PDCommon/Kernel/include/Stabilizer.h
    git commit -m "feat: add damageCoupling_ config and setter in Stabilizer base class"
    ```

---

### Task 2: 机械稳定器受力计算损伤场耦合 (MechanicalStabilizers.cpp)

**Files:**
- Modify: `PDCommon/Kernel/src/MechanicalStabilizers.cpp`

- [x] **Step 1: 编写检查机制（Failing Log Test）**
  - 目前的稳定器并不依赖 `Damage_Trial`。我们使用 `git diff` 确认当前 `MechanicalStabilizers.cpp` 没有获取 `Damage_Trial`。
- [x] **Step 2: 确认无损伤场提取行为**
  - 检查 `MechanicalStabilizers.cpp` 中不含有 `Damage_Trial` 关键字。
- [x] **Step 3: 在三个稳定器中实现 `Damage_Trial` 耦合计算**
  - **Silling 稳定器** (`MechanicalSillingStabilizer::applyPenalty`)：
    - 获取 `Damage_Trial` 场指针并结合到粒子循环内计算最终的 `final_softening`：
      ```cpp
      // 提取 Damage_Trial 场
      auto *damageField = fieldManager.getFieldAs<double>("Damage_Trial");
      const double *damagePtr = damageField ? damageField->dataPtr() : nullptr;
      ...
      // 在循环末尾：
      double damage_val = (damageCoupling_ && damagePtr) ? damagePtr[i] : 0.0;
      double damage_softening = std::max(0.0, 1.0 - damage_val);
      double final_softening = plastic_softening * damage_softening;
      accPtr[i * 3] += (force_x * final_softening) / rho_i;
      accPtr[i * 3 + 1] += (force_y * final_softening) / rho_i;
      accPtr[i * 3 + 2] += (force_z * final_softening) / rho_i;
      ```
  - **Wan 稳定器** (`MechanicalWanStabilizer::applyPenalty`)：
    - 进行同样的损伤因子提取与计算逻辑，将 `plastic_softening` 升级为结合 `damage_softening` 的 `final_softening` 并应用到 `accPtr` 的计算中。
  - **Zhang 稳定器** (`MechanicalZhangStabilizer::applyPenalty`)：
    - 结合 `reduction_factor` 计算最终软化：
      ```cpp
      double damage_val = (damageCoupling_ && damagePtr) ? damagePtr[i] : 0.0;
      double damage_softening = std::max(0.0, 1.0 - damage_val);
      double final_reduction = reduction_factor * plastic_softening * damage_softening;
      accPtr[i * 3] += (force_x * final_reduction) / rho_i;
      accPtr[i * 3 + 1] += (force_y * final_reduction) / rho_i;
      accPtr[i * 3 + 2] += (force_z * final_reduction) / rho_i;
      ```
- [x] **Step 4: 编译验证**
  - 运行：`cmake --build build --config Release`
  - 预期：编译 PASS 成功
- [x] **Step 5: Git Commit**
  - 运行：
    ```bash
    git add PDCommon/Kernel/src/MechanicalStabilizers.cpp
    git commit -m "feat: couple Damage_Trial (1-d) into Silling, Wan, and Zhang stabilizers"
    ```

---

### Task 3: 求解器 YAML 参数解析与传递 (NOSB 求解核)

**Files:**
- Modify: `PDCommon/Kernel/include/NOSB_Base.h`
- Modify: `PDCommon/Kernel/src/NOSB_Base.cpp`
- Modify: `PDCommon/Kernel/src/NOSB_M.cpp`
- Modify: `PDCommon/Kernel/src/NOSB_Axis.cpp`
- Modify: `PDCommon/Kernel/src/NOSB_T.cpp`
- Modify: `Examples/Axisymmetric_Notched_Bar/PD.yaml`

- [x] **Step 1: 在 YAML 中添加参数并验证尚未被解析**
  - 在 `Examples/Axisymmetric_Notched_Bar/PD.yaml` 中添加：
    ```yaml
    ZeroEnergyDamageCoupling: true
    ```
- [x] **Step 2: 验证编译并检查当前无法解析该参数**
  - 编译并使用测试日志审查：在 `NOSB_Base::configure` 中尚不解析 `ZeroEnergyDamageCoupling`。
- [x] **Step 3: 实现 YAML 参数解析与多态传递**
  - 在 `NOSB_Base.h` 增加 `bool zeroEnergyDamageCoupling_{true};`。
  - 在 `NOSB_Base.cpp` 的 `configure` 中解析：
    ```cpp
    if (solverNode["ZeroEnergyDamageCoupling"]) {
      zeroEnergyDamageCoupling_ = solverNode["ZeroEnergyDamageCoupling"].as<bool>();
      LOG_INFO("[NOSB_Base] Applied ZeroEnergyDamageCoupling: " +
               std::to_string(zeroEnergyDamageCoupling_));
    }
    ```
  - 在 `NOSB_M.cpp`, `NOSB_Axis.cpp`, `NOSB_T.cpp` 的稳定器实例化部分，调用 Setter：
    ```cpp
    stabilizer_->setDamageCoupling(zeroEnergyDamageCoupling_);
    ```
- [x] **Step 4: 编译并通过系统算例加载校验**
  - 运行：`cmake --build build --config Release`
  - 预期：编译通过。
- [x] **Step 5: Git Commit**
  - 运行：
    ```bash
    git add PDCommon/Kernel/include/NOSB_Base.h PDCommon/Kernel/src/NOSB_Base.cpp PDCommon/Kernel/src/NOSB_M.cpp PDCommon/Kernel/src/NOSB_Axis.cpp PDCommon/Kernel/src/NOSB_T.cpp Examples/Axisymmetric_Notched_Bar/PD.yaml
    git commit -m "feat: parse and pass ZeroEnergyDamageCoupling YAML parameter to stabilizers"
    ```
