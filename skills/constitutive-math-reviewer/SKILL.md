---
name: constitutive-math-reviewer
description: "GRPD 本构数学专项入口。用于评审连续介质力学公式、近场动力学应力更新、Voigt/张量约定、J2/JC 塑性、热传导、PK1/PK2/Cauchy 应力、切线刚度、平面应力/平面应变/轴对称公式和 Eigen 实现。"
---

# GRPD 本构数学入口

使用中心工具包：`../grpd-cae-toolkit`。

## 必做流程

1. 读取 `../grpd-cae-toolkit/references/material-state.md`。
2. 读取 `../grpd-cae-toolkit/references/validation-playbook.md`。
3. 如改动涉及 Kernel 热循环，再读取 `../grpd-cae-toolkit/references/performance-openmp.md`。

## 判断重点

- 先写出代码实现的数学表达式，再判断公式是否正确。
- 明确应变定义、应力类型、张量/Voigt 顺序和剪切因子。
- 对 J2/JC 检查屈服函数、返回映射、硬化项、状态变量和输出量。
