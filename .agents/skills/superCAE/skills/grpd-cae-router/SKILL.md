---
name: grpd-cae-router
description: "Use when GRPD tasks touch CAE/C++ subsystems, including materials, solver loops, kernels, OpenMP, mesh/neighbor/contact, YAML input, VTK output, numerical validation, smoke tests, or ANSYS/MATLAB comparison workflows."
---

# GRPD CAE Router

本 skill 是 GRPD 的 CAE 专项路由层。它不替代任何专项 skill，只负责在 Superpowers 流程中选择正确的 GRPD 领域技能、风险检查和验证路径。

## 核心原则

先判定 CAE 影响面，再进入实现或评审。任何涉及 C++、材料、本构、求解器、Kernel、OpenMP、输入输出或数值验证的任务，都应先用本路由层确定需要加载的专项 skill。

## 路由流程

1. 读取用户请求、计划文件或当前 diff，列出可能触及的模块。
2. 按下方矩阵选择最小必要 skill 集合。
3. 在输出中声明：`[Active Skill: grpd-cae-router + <专项 skill>]`。
4. 读取所选专项 skill 的 `SKILL.md`，必要时再读取其 `references/`。
5. 对每个任务写清楚：影响的 8 层管线、状态变量风险、SoA 字段风险、并行风险、验证命令。
6. 若证据不足，先运行 `grpd-cae-toolkit` 的脚本收集事实，再下结论。

## 路由矩阵

| 任务症状/改动区域 | 必读专项 skill | 重点检查 |
|---|---|---|
| 新增或修改材料、本构、塑性、损伤变量 | `material-model-implementer`、`constitutive-math-reviewer`、`numerical-test-generator` | 参数读取、状态变量分配、Trial/Old/Commit、公式与 Voigt 约定 |
| 修改 `TimeIntegrator`、ADR、隐式/JFNK/L-BFGS、载荷步 | `solver-loop-safety-reviewer`、`numerical-test-generator` | 内外循环隔离、状态冻结、边界施加顺序、收敛后提交 |
| 修改 `PDKernel`、NOSB、稳定器、力/热流积分 | `openmp-kernel-optimizer`、`constitutive-math-reviewer`、`mesh-and-neighbor-reviewer` | SoA 裸指针、邻域遍历、OpenMP 数据竞争、零能稳定 |
| 修改网格、粒子、邻域、接触搜索、摩擦 | `mesh-and-neighbor-reviewer`、`openmp-kernel-optimizer` | CSR/Cell List、一致的粒子 id、接触量纲、摩擦冲量限幅 |
| 修改 `PD.yaml`、输入解析、默认值、错误信息 | `cae-input-schema-validator` | schema、单位量纲、默认值、文档一致性 |
| 修改物理场注册、字段生成、FieldManager | `physics-field-pipeline-auditor`、`grpd-cae-toolkit` | Physics Type 到 FieldManager 链路、SoA 连续数组、字段命名 |
| 修改 Writer、VTK、PVD、输出变量 | `postprocess-exporter`、`grpd-cae-toolkit` | Writer.Variables、标量/向量/张量输出、ParaView 可读性 |
| 修改 CMake、源文件组织、第三方依赖 | `cmake-build-doctor` | C++17、OpenMP、对象库、include 路径、源文件注册 |
| 需要回归、Patch Test、解析解对比 | `numerical-test-generator`、`grpd-smoke-tester` | 最小算例、期望结果、失败判据、是否需要 ANSYS/MATLAB |
| 需要全链路冒烟或误差报告 | `grpd-smoke-tester`、`grpd-experiment-tracker` | GRPD VTK、ANSYS TXT/DB、路径对比、误差入库 |

## 计划与评审必填字段

在 Superpowers 计划、执行记录或代码评审中，GRPD 任务至少写出这些字段：

```markdown
**Touched Pipeline Layers:** 层 1/2/3/4/5/6/7/8 中的具体层级
**Domain Skills:** 本任务实际读取并遵循的专项 skill
**State Risk:** 是否涉及 Old/Trial/Commit、stateMode、损伤正式提交
**SoA Risk:** 是否新增或读取 FieldManager 字段，是否在核心循环中使用连续裸指针
**Parallel Risk:** 是否涉及 OpenMP、共享写、归约、缓存局部性
**Validation:** 构建命令、最小数值算例、冒烟测试或对标工具
```

## 验证选择

- 结构/注册/字段问题：优先运行 `grpd-cae-toolkit/scripts/*.py` 中的专项检查脚本。
- CMake 或编译链问题：优先使用 `cmake-build-doctor`，然后运行最小构建命令。
- 材料本构问题：优先做单点路径验证；需要解析参考时调用 MATLAB 对标。
- 求解器或 Kernel 问题：先做最小 GRPD 算例，再按需要升级到 GRPD/ANSYS 路径对比。
- 参数扫描或算法比较：使用 `grpd-experiment-tracker` 记录 residual、耗时、误差和 artifact。

## 禁止事项

- 不要把材料状态变量创建逻辑散落到求解器或 Kernel 中。
- 不要绕过 `FieldManager` 手工创建高频物理场。
- 不要在隐式内循环未收敛时提交历史状态或正式断键。
- 不要只用大算例证明正确性；必须先有最小可解释验证。
- 不要为使用 Superpowers 而绕开 GRPD 的 8 层管线和专项 skill。
