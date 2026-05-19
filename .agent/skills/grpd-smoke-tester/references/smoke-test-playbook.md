# GRPD 冒烟测试与验证工作流 (Smoke Test Playbook)

## 核心目标
保证 GRPD 的 C++ 计算核心（如本构模型更新、断裂准则修正、积分核优化）在经历修改后，仍然能在基础 Benchmark (如轴对称拉伸、断裂测试) 上输出与 ANSYS 商业软件对标的精确结果。

## 工具流转
自动化冒烟测试强依赖于 Python 驱动脚本和 MCP 服务器的联合：
1. **构建模块**：`cmake` 确保拿到包含最新代码的可执行文件。
2. **求解模块**：执行 `PD.exe` 获得当前模型的 `.vtk` 文件。
3. **验证模块 (`ansys-server`)**：通过注入参数，动态获取商软的基准切片数据。
4. **分析模块 (`path-comparison-server`)**：自动滤除无用点云，对齐坐标系，进行插值对比。

## 注意事项
- **文件锁定**：GRPD 的输出是带时间戳的文件夹 `Result_YYYYMMDD_HHMMSS`。测试脚本必须能自适应寻址最新的目录和 `.vtk` 文件。
- **坐标提取边界**：点云数据切片由于没有拓扑关系，在 `path-comparison-server` 的底层代码中通过容差过滤（如 `abs(x - target_x) < tol`）。务必确保截取路径在几何范围内。
- **验证判定标准**：
  - 弹性段位移/应力误差应极小 (`< 0.1%`)。
  - 塑性段应力误差由于积分方式不同，通常容忍度为 `< 5%`。
  - 断裂段判定应观察应力是否突降。

## 一键脚本调用示例
当 AI (Codex) 执行测试任务时，可以直接调用内置的自动化脚本：
```bash
python .agent/skills/grpd-cae-toolkit/scripts/run_smoke_test.py \
    --build-dir "D:\C++pro\GRPD\build" \
    --test-dir "D:\C++pro\GRPD\Examples\Axisymmetric_Ring" \
    --mac-file "D:\C++pro\GRPD\Examples\Axisymmetric_Ring\ansys_val.mac" \
    --ansys-workdir "D:\ANSYS_Project\GRPD_AXIS" \
    --start-x 1.0 \
    --end-x 1.0
```
