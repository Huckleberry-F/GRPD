---
name: constitutive-math-reviewer
description: "GRPD 本构数学专项入口。用于评审连续介质力学公式、近场动力学应力更新、9分量张量约定、J2/JC 塑性、热传导、PK1/PK2/Cauchy 应力、切线刚度、平面应力/平面应变/轴对称公式、Eigen 实现，以及通过 point-integration-matlab-server 做材料单点积分参考验证。"
---

# GRPD 本构数学与单点积分联合校验手册

本技能提供了一套全自动、解耦的材料本构联合校验工作流。由 `matlab-mcp-server` 提供，利用 FastMCP 工具 `run_constitutive_validation` 驱动双端进行等效加载，实现 C++ 求解器与 MATLAB 解析解的无偏对标。

## 一键执行对标校验

你可以通过调用 `PointIntegrationMatlabServer` MCP 服务的工具：
- **`run_constitutive_validation(parameters, F_path, work_dir)`**：一键执行本构对标校验。它会自动完成 C++ 端的增量编译与运行、MATLAB 积分脚本的生成与求解，以及双端结果误差比对和绘图。

## 校验工作流机制

1. **变形梯度驱动**：双端直接接收指定的变形梯度路径 `F_path`（$N \times 9$ 行优先矩阵）进行力学求解。
2. **大变形模式下**：
   - 采用 Green-Lagrange 应变：
     $$\boldsymbol{E} = 0.5(\boldsymbol{F}^T \boldsymbol{F} - \boldsymbol{I})$$
   - 提取并转换 Cauchy 应力为第一类 Piola-Kirchhoff (PK1) 应力：
     $$\boldsymbol{P} = \boldsymbol{F} \boldsymbol{\sigma}$$
   - 双端在 PK1 应力的 9 分量全张量形式上进行比对。
3. **小变形模式下**：
   - 采用小应变：
     $$\boldsymbol{\epsilon} = 0.5(\boldsymbol{F} + \boldsymbol{F}^T) - \boldsymbol{I}$$
   - 双端直接在 Cauchy 应力 $\boldsymbol{\sigma}$ 的 9 分量全张量形式上进行比对。
4. **张量与积分约定**：
   - 所有的材料本构积分计算与对标比对**严禁使用任何形式的工程分量或 Voigt 表示**。应力、应变和变形梯度一律表示为 9 分量行优先序列。
5. **误差要求**：
   - 应力绝对误差值限制为 $1.0 \times 10^{-5}$ Pa（处理大应力下的尾数浮点抖动）。
   - 塑性与损伤等状态变量误差限制为 $1.0 \times 10^{-10}$（要求机器双精度匹配）。

## 关键参考文献

- 详细物理与约定说明：[point-integration-validation.md](file:///d:/Project_C++/GRPD/.gemini/skills/constitutive-math-reviewer/references/point-integration-validation.md)
