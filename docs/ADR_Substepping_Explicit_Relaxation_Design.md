# 极度非线性重装积分器：基于子步分段的显式动态松弛跟踪法 (Substepping Explicit Dynamic Relaxation)

## 1. 算法缘起与痛点分析
当前绝大多数非线性隐式或准静态松弛算法（包括经典的 ADR 算法），通常嵌套两层循环：外层定义目标载荷 (`LoadStep` & `Substep`)，内层 (`iter`) 负责通过无阻尼或带阻尼震荡寻找对应于该载荷的力学平衡点。

**核心痛点**：因为载荷在内层循环起始时（如 $N=1\to N=2$ 时）有一个绝对的“突跃”（Jump），系统会在最初的几次求根迭代中引发巨大的虚拟节点位移超调。如果材料具有复杂的**径向返回算法 (Radial Return Mapping)**（如强硬化、裂纹屈服等），这种不真实的瞬态高应变会直接导致本构模型积分发散或发生不可逆的数学截断错误，进而导致整个宏观步骤无法收敛。

## 2. 核心突破性思想 (The Breakthrough Concept)
为了根除“突跃”导致的本构崩溃，该构思将**显式加载 (Ramp Loading)** 和 **状态立即固化 (Immediate State Commit)** 直接下放并融合到虚拟时间（`iter` 伪时间步）中。

### 2.1 混合架构设计
1. **宏观控制 (LoadStep & Substep)**：保留外层控制循环以定义大阶段或输出节点（例如 $t_{start} \to t_{end}$ 划分为 $N$ 个 Substep）。
2. **渐近爬坡加载 (In-iter Ramp)**：在任何一个 `Substep` 内部，边界条件或载荷绝不瞬间达到 `target_load`。而是在 `iter` 进行的前 $M$ 步（爬坡期）中，通过插值平滑上升：$F(iter) = F_{prev} + \Delta F \times (iter / M)$。
3. **当场固化理论 (Immediate Commit Theory)**：由于施加载荷的极其缓慢（结合 ADR 的自适应动能耗散），系统在每一个虚时间步 $iter$ 上都几乎严密贴合着真实的“准静态物理加载路径”。由于贴合物理真实，这使得我们在 $iter$ 每步计算径向返回后，可以直接调用 `commitState()`。

## 3. 标准处理流程图
```yaml
For LoadStep = 0 to N:
  For Substep = 1 to M:
     Load_Prev = ...
     Load_Target = ...

     // === 内层松弛循环 ===
     While (iter < MaxPseudoSteps and NOT Converged):
         // 1. 动态渐进加载 (消灭瞬态冲击)
         If iter < RampSteps:
             Load_current = Lerp(Load_Prev, Load_Target, iter / RampSteps)
         Else:
             Load_current = Load_Target
         
         ApplyBoundaryConditions(Load_current)
         
         // 2. 本构计算 (单步极小应变，100% 成功越过屈服面)
         EvaluateForces() => F_internal，触发径向返回算法
         
         // 3. 动态松弛 (计算自适应阻尼并吸干当前微小增量带来的动能)
         Compute_Adaptive_Damping(cn)
         Leapfrog_Update_Disp_Vel()
         
         // 4. 重磅修改！当场固化历史变量！
         CommitState() 

         // 5. 爬坡期结束后的收敛监测
         If iter > RampSteps and TOL_is_met:
             Converged = true
```

## 4. 工程与物理优势
- **永远没有“不收敛”的尴尬**：只要爬坡期 (`RampSteps`) 足够长，内部应变增量就无限小，任何极其变态的塑性或者大变形本构都能像显式动力学那样顺滑流淌过去，无惧径向返回截断。
- **全物理状态追溯**：传统的 ADR 只在 `Substep` 结束时拥有真正的物理意义。而本构思想让 `iter` 的每一步都成为了极其精准的微秒级慢镜头重放。
- **灵活性拉满**：对于不需要极高非线性容错的场景，可以通过 YAML 令 `RampSteps = 0`，则算法秒变回传统的寻找定点根模式 (Standard Root Finding ADR)。

## 5. 待开发落地指南
本构思可作为 `ADR_Integrator` 的高阶进阶版本（如命名为 `ADR_AdvancedNonlinear` 或是作为配置标志 `EnableIterRamp=True`）在之后的迭代中实装。它仅需要：
1. `ADR_Integrator` 类中引入 `RampPseudoSteps` 参数。
2. 剥离或位移现有的 `commitState()` 调用层级到内部 `iter` 尾部。
3. 调整原有的基于全局 $\Delta U$ 的阻尼 $c_n$ 计算方式，确保其适应这种连续滑移式的位移变化体系。

*(This document serves as the architectural blueprint for the next-generation highly-nonlinear explicit quasi-static solver in the GRPD framework. Conceived by the User on 2026-04-02)*
