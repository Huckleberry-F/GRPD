# 🚀 实施计划：GRPD 自适应隐显断裂切换架构 (Adaptive IM-EX Fracture)

本计划旨在重构 `ADR_Integrator.cpp`，彻底解决静态 NR 迭代与断裂拓扑更新之间的冲突，实现工业级商软的“试探损伤 -> 收敛固化”机制，并加入独创的“智能降级回滚”功能。

## 🎯 核心目标
1. **彻底解耦**：将不可逆的拓扑断裂 (`postCompute`) 与寻找静态平衡的 NR 迭代过程完全剥离。
2. **黄金标准 (Strategy 2)**：引入 `ImplicitConverged` 策略，实现“基于 Trial 损伤寻找平衡，收敛后固化断裂”的隐式标准流程。
3. **自适应降级 (Auto-Switch)**：当结构发生全局撕裂（NR 无法收敛）时，自动回滚状态，无缝切换到 `FastInnerLoop` 纯动态显式模式。

## 🔧 拟修改文件列表

### 1. [MODIFY] `Src/Integration/src/ADR_Integrator.cpp`
**修改点一：外层包装 Retry 循环**
在原有的子步循环 (`for sub < currentNumSubsteps`) 内部，包裹一层 `while(!substepComplete)` 循环。该循环负责处理隐式断裂引发的重新平衡，以及不收敛时的回滚重试。

**修改点二：新增 `ImplicitConverged` 断裂策略**
* 在 NR 外循环完全收敛 (`NRConverged == true`) 后：
  1. 调用 `kernel->postCompute(ctx)` 执行真正的不可逆断键。
  2. 如果检测到有键断裂（拓扑改变），则将 `NRConverged` 置回 `false`，停留在当前子步，利用刚才的变形作为初值，**重新启动本子步的 NR 迭代**以寻找断键后的新平衡（外-外循环）。
  3. 如果没有键断裂，则本子步彻底完成 (`substepComplete = true`)。

**修改点三：自适应降级与回滚逻辑**
* 当 `NRIter == maxNRIters_` 且依然不收敛时：
  1. 拦截失败信号，打印炫酷的 `[Auto-Switch]` 警告。
  2. 将粒子当前位移 `uPtr` 全部重置为 `dispBase_`，清零 `velHalfOld_`。
  3. 强行修改配置：`maxNRIters_ = 1`（关闭 NR），`NRStateFrozen_ = false`（实时更新），`fractureStrategy_ = "FastInnerLoop"`（极速显式断裂）。
  4. 利用 `continue` 重新执行该子步，让显式动力学硬推过断裂极值点。

### 2. [MODIFY] `Examples/Plastic_Fracture/PD.yaml`
**修改点**
* 将 `Fracture_Strategy` 修改为 `"ImplicitConverged"` 以测试新功能。
* （已提前将 `Damage_Trial` 更改为 `Damage` 输出）。
