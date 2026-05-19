---
name: constitutive-math-reviewer
description: "GRPD 本构数学专项入口。用于评审连续介质力学公式、近场动力学应力更新、Voigt/张量约定、J2/JC 塑性、热传导、PK1/PK2/Cauchy 应力、切线刚度、平面应力/平面应变/轴对称公式、Eigen 实现，以及通过 point-integration-matlab-server 做材料单点积分参考验证。"
---

# GRPD 本构数学入口

使用中心工具包：`../grpd-cae-toolkit`。

## 必做流程

1. 读取 `../grpd-cae-toolkit/references/material-state.md`。
2. 如需要设计最小验证算例，读取 `../numerical-test-generator/references/validation-playbook.md`。
3. 如改动涉及 Kernel 热循环，再读取 `../openmp-kernel-optimizer/references/performance-openmp.md`。
4. 如任务涉及材料单点积分、返回映射、状态变量更新或 C++/参考解对比，读取 `references/point-integration-validation.md`，并优先调用 `point-integration-matlab-server`。

## 判断重点

- 先写出代码实现的数学表达式，再判断公式是否正确。
- 明确应变定义、应力类型、张量/Voigt 顺序和剪切因子。
- 对 J2/JC 检查屈服函数、返回映射、硬化项、状态变量和输出量。
- 不要只凭公式印象判断本构积分；构造最小 total strain path，调用 `point-integration-matlab-server.run_j2_uniaxial_path` 或 `run_j2_strain_path` 得到参考响应，再对比 C++ 的 stress、eqp、damage 或其他状态变量。
- 对比失败时，优先检查 Voigt 顺序、剪切因子、应变增量/总应变、屈服函数符号、塑性乘子分母、状态变量 commit 顺序和单位。
