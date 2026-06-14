# ANSYS 瞬态子步数 (NSUBST) 收敛性扫描测试计划

## 1. 测试目标
固定 GRPD 求解器在 $t=0.02\text{ s}$（$dt=10^{-5}\text{ s}$，运行 2000 步）下的原始计算结果：
`d:/C++pro/GRPD/Examples/Thermal_Validation_5x10/Result_20260613_142529/Thermal_Validation_5x10_step02000_t0.0200.vtk`

在 ANSYS 侧，保持二阶 Crank-Nicolson 隐式积分（$\theta=0.5$）和原始面热流载荷（HFLUX = 100.0）一致，扫描其子步数 $NSUBST \in [100, 500, 1000, 2000, 5000]$。分析两点：
1. ANSYS 温度场曲线随着时间步长 $dt_{ANSYS}$ 缩短是否单调收敛。
2. 当 $dt_{ANSYS}$ 与 GRPD 的 $10^{-5}\text{ s}$ 完全对齐（NSUBST = 1000）以及进一步加密（NSUBST = 2000, 5000）时，在边界处的误差与分布形态有何改变。

---

## 2. 自动化执行机制
由于需要运行 5 次“求解 + 对标”，我们在 `scratch/nsubst_sweep.py` 中编写自动化批处理脚本：
1. 以已生成且经过二阶 CN 积分和面热流加载修改的 [ansys_smoke_test.mac](file:///D:/C++pro/GRPD/.agents/mcp/ansys-mcp-server/work_dir/run_0066/ansys_smoke_test.mac) 作为基础模板。
2. 循环遍历 $N \in [100, 500, 1000, 2000, 5000]$，动态改写宏文件中的 `NSUBST, 1000, 1000, 1000` 语句为 `NSUBST, N, N, N`。
3. 调用 `ansys-server.run_ansys_mac` 求解，并将输出的文本数据与 GRPD VTK 进行插值对比（调用 `grpd-validation-server.compare_grpd_vtk_with_ansys_txt`）。
4. 汇总各步数下的表面温度 $T(0.0)$ 以及最大误差，并绘制多曲线收敛对比图。

---

## 3. 执行与验证计划
- [ ] 1. 物理写入 `scratch/nsubst_sweep.py` 自动化执行脚本。
- [ ] 2. 在工程终端运行该 Python 脚本，完成 5 组循环对标。
- [ ] 3. 统计并整理输出数据。
- [ ] 4. 物理落盘 Walkthrough 报告，并向用户展示结果与收敛曲线结论。
