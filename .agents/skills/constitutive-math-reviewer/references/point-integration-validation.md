# 本构单点积分验证

## 目标

用 `point-integration-matlab-server` 为 GRPD 材料模型提供独立参考响应，优先验证小变形 J2 返回映射、硬化律、状态变量提交和 Voigt 约定。

## MCP 工具

- `point-integration-matlab-server.check_matlab_available`：确认 MATLAB 是否在当前机器可用。
- `point-integration-matlab-server.run_j2_uniaxial_path`：对 xx 单轴 total strain path 做 J2 线性各向同性硬化积分。
- `point-integration-matlab-server.run_j2_strain_path`：对 6 分量 total strain path 做 J2 线性各向同性硬化积分。

## 推荐流程

1. 阅读目标 C++ 材料类，确认 stress 类型、Voigt 顺序、剪切分量约定、材料参数和状态变量名称。
2. 构造最小 strain path，先用弹性步确认弹性矩阵，再增加越过屈服的塑性步。
3. 调用 `point-integration-matlab-server` 得到参考结果。
4. 将参考结果与 C++ 单点测试、最小算例输出或调试日志中的 stress/state 对比。
5. 若误差出现于弹性段，优先检查单位、Lamé 常数、剪切因子和 Voigt 顺序。
6. 若误差只出现于塑性段，优先检查屈服函数、返回映射分母、硬化项、等效塑性应变增量和状态变量提交。

## 当前约定

- Voigt 顺序：`[xx, yy, zz, xy, yz, zx]`。
- 剪切分量：tensor strain，不是 engineering shear strain。
- 当前 MCP 第一版内置 J2 线性各向同性硬化参考；复杂 JC、损伤或有限变形模型应先补充参考实现，再做自动结论。
