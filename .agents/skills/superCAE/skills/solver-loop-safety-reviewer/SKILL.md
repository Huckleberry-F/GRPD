---
name: solver-loop-safety-reviewer
description: "GRPD 求解循环专项入口。用于评审 PDEngine 初始化顺序、TimeIntegrator、载荷步、边界条件施加顺序、Kernel 调用、状态变量提交、输出回调、ADR/CentralDifference/MatrixFreeImplicit 等流程的正确性和回归风险。"
---

# GRPD 求解循环入口

使用中心工具包：`../grpd-cae-toolkit`。

## 必做流程

1. 读取 `../grpd-cae-toolkit/references/architecture.md`。
2. 读取 `../grpd-cae-toolkit/references/solver-loop.md`。
3. 若涉及字段状态，运行 `python .agents/skills/superCAE/skills/grpd-cae-toolkit/scripts/check_field_references.py .`。

## 判断重点

- 初始化顺序是否被破坏。
- BC、Kernel、状态变量 swap/commit、输出回调的时间顺序是否正确。
- 时间循环中不要引入不必要的 YAML 读取、动态分配或跨模块硬耦合。
