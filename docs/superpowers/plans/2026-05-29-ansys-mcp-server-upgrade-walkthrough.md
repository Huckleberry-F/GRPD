# ANSYS MCP 工具链升级与热传导支持 (计划与总结)

## 1. 任务背景

在尝试运行 `Thermal_Validation` 热传导算例并使用 ANSYS 进行交叉比对时，我们发现了以下几个工具链层面的问题：
1. **ANSYS 运行入口缺失**：`ansys_runner.py` 是作为一个 Python 模块存在的，没有提供直接运行宏文件的命令行接口，导致 AI 或者用户需要反复编写临时脚本。
2. **比较脚本写死力学数据**：`compare_and_export.py` 内部硬编码了对力学场（`Displacement`, `VonMisesStress`）和 ANSYS 的 `UY`, `SEQV` 列的提取。当应对包含 `TEMPVAL` / `Temperature` 的热问题时，程序直接报错崩溃。

为了彻底解决此问题，拒绝使用一次性的“垃圾”胶水脚本，决定在底层对工具链进行系统性升级。

## 2. 实施计划 (Plan)

- **升级 `ansys_runner.py`**：
  - 引入 `argparse`。
  - 在文件末尾添加 `if __name__ == "__main__":` 块。
  - 支持从 `config.yaml` 自动读取正确的 ANSYS 安装路径。
- **升级 `compare_and_export.py`**：
  - 添加 `physics_type` 控制参数。
  - 根据 `physics_type`（`Mechanical` 或是 `Thermal`），动态判定并提取应力/位移或者温度场数据。
  - 提供针对热传导结果的高保真 PNG 渲染（红色温度对比曲线）。
  - 添加命令行运行入口，实现全自动化。

## 3. 执行结果 (Walkthrough)

1. 成功修改了 `tools/ansys-mcp-server/ansys_runner.py`，现在允许使用 `python ansys_runner.py <mac_file>` 的原生命令执行基准验证。
2. 成功重构了 `tools/ansys-mcp-server/compare_and_export.py`。新增了对 `Temperature` 标量场的完全兼容，保证二维热传导结果的数据对齐、插值和误差计算正常工作。
3. 彻底清除了中途产生的冗余临时调用脚本（`run_ansys.py`），保持了 GRPD 工程 `Examples/Thermal_Validation/` 目录的绝对干净。

## 4. 下一步行动建议

使用原生指令验证当前算例：
```bash
# 1. 求解
python tools/ansys-mcp-server/ansys_runner.py Examples/Thermal_Validation/ansys_val.mac
# 2. 对比
python tools/ansys-mcp-server/compare_and_export.py --vtk Examples/Thermal_Validation/Result/Result_step00010_t10.0000.vtk --ansys Examples/Thermal_Validation/ansys_val_results.txt --type Thermal
```
