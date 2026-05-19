---
name: grpd-smoke-tester
description: "GRPD 自动化冒烟测试与验证入口。用于在修改 C++ 源码（如本构模型、积分核）后，一键执行全流程验证：驱动 GRPD 编译求解、调度 ansys-server 提取基准解、调用 path-comparison-server 进行数据切片融合与误差图表生成。"
---

# GRPD 自动化冒烟测试 (Smoke Test)

使用中心工具包：`../grpd-cae-toolkit`（如有需要）。

## 必做流程

1. 读取 `references/smoke-test-playbook.md` 熟悉测试原理与评判标准。
2. 确认当前系统已连接 `ansys-server` 和 `path-comparison-server` 两个 MCP 服务器。
3. 使用中心工具包的 Python 脚本一键驱动全流水线（编译 -> GRPD计算 -> ANSYS参数注入 -> 切片对标）：
   ```bash
   python .agent/skills/grpd-cae-toolkit/scripts/run_smoke_test.py \
       --build-dir "D:\C++pro\GRPD\build" \
       --test-dir "D:\C++pro\GRPD\Examples\Axisymmetric_Ring" \
       --mac-file "D:\C++pro\GRPD\Examples\Axisymmetric_Ring\ansys_val.mac" \
       --ansys-workdir "D:\ANSYS_Project\GRPD_AXIS" \
       --start-x 1.0 \
       --end-x 1.0
   ```
4. 如果脚本执行成功，将输出的对比图 (如 `Comparison_Plot.png`) 以及最终的误差百分比展示给用户，并宣告 **[PASS]**。
5. 如果误差过大或发生数值崩溃报错，宣告 **[FAIL]**，并结合 C++ 本构改动分析原因。

## 判断重点

- 务必确保最新编译的 `GRPD.exe` 被实际执行，防止读取旧的缓存数据。
- 传入 `--start-x` 坐标时，需确保该坐标处于几何物理范围内。
