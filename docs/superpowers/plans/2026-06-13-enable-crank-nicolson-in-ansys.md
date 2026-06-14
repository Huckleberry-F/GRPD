# 在 ANSYS 瞬态热分析中启用二阶 Crank-Nicolson 积分并重新对标计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在 `ansys-mcp-server` 生成的 APDL 中为瞬态热分析启用二阶 Crank-Nicolson 积分算法（THETA=0.5），以消除大步长下的数值阻尼耗散，重新执行 $t=2.0\text{ s}$ 时刻中线的 GRPD 与 ANSYS 精度对比。

**Architecture:** 
1. 在 `generator.py` 的瞬态设置部分，当检测到为热学分析时，向 APDL 插入 `TINTP, , 0.5` 指令。
2. 重新运行 `ansys-server` 求解算例并导出 $t=2.0\text{ s}$ 时的温度分布结果。
3. 调度 `grpd-validation-server` 进行插值对比，验证启用二阶积分后 ANSYS 结果是否克服了数值阻尼并更紧密地重合 GRPD 红线。
4. 归档实验数据并物理落盘 Walkthrough 总结。

**Tech Stack:** Python MCP Services, APDL, Heat Transfer Numerical Methods

---

## 实施步骤

### Task 1: 修改 APDL 生成代码

**Files:**
- Modify: `d:/C++pro/GRPD/.agents/mcp/ansys-mcp-server/src/generator.py:174-178`

- [ ] **Step 1: 插入 TINTP 积分参数指令**
  在 `generator.py` 写入 `ANTYPE, TRANS` 之后，如果是热学分析，插入 `TINTP, , 0.5` 以启动二阶 Crank-Nicolson 算法。

```python
    if is_transient:
        apdl += [
            "ANTYPE, TRANS",      # 瞬态分析
            "TIMINT, ON",         # 开启时间积分
        ]
        if is_thermal:
            apdl += [
                "TINTP, , 0.5",   # 启用 Crank-Nicolson 二阶无阻尼积分
            ]
```

---

### Task 2: 执行重新对标与验证

- [ ] **Step 1: 重新定位 GRPD 的 VTK 路径**
  调用 `grpd-server.get_grpd_vtk_result`（直接在已有目录中获取 step 20000 对应的 VTK 文件）。
  
- [ ] **Step 2: 重新运行 ANSYS 并提取 t=2.0s 结果**
  调用 `ansys-server.run_ansys_yaml_case`，指定参数：
  * `time = 2.0`
  * `start_x = 0.0`, `end_x = 5.0`
  * `start_y = 5.0`, `end_y = 5.0`
  
- [ ] **Step 3: 运行插值对比与误差分析**
  调用 `grpd-validation-server.compare_grpd_vtk_with_ansys_txt` 对标 `components = ["TEMP"]`。

- [ ] **Step 4: 数据库入库与 Walkthrough 记录**
  调用 `grpd-experiment-server` 创建并结束实验记录，物理写入 Walkthrough 报告。
