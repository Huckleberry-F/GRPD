# 新增多载荷步稳态隐式热传导验证算例实现计划 (Implementation Plan)

## 任务背景与目标
用户希望在 `Examples` 目录下新建一个文件夹，使用 5x10 几何算例的模板，将其修改为稳态隐式 JFNK 求解。具体要求：
1. 载荷步数量为 2（`LoadSteps` 的数量为 2），每一载荷步有 5 个子步（`NumSubsteps: 5`）。
2. 第一载荷步（Step 1）：左端 Neumann 热流边界设为 50，右端 Dirichlet 温度边界同步阶梯更新为 50。
3. 第二载荷步（Step 2）：左端 Neumann 热流边界设为 100，右端 Dirichlet 温度边界同步阶梯更新为 100。
4. 在 `Examples/` 目录下生成该算例并运行验证，检验求解器的收敛情况与稳态递增计算的正确性。

---

## 拟新建/修改文件

### 1. 新建算例文件夹与 YAML 配置
#### [NEW] [PD.yaml](file:///Users/hanbozhang/C++/GRPD/Examples/Thermal_Validation_Steady_Steps/PD.yaml)
配置 5x10 算例的基本几何与材料信息，并将 `Solver` 配置为稳态隐式：
*   `TimeIntegrator: "MatrixFreeImplicit"`
*   `Algorithm: "JFNK"`
*   2 步载荷步，每步 `NumSubsteps: 5`。
*   配置 `LoadStep_Conditions` 以支持在不同的 Step 中阶梯式改变边界条件的值。

---

## 验证与执行方案

### 1. 模型预处理
执行前处理脚本来生成网格和步边界二进制数据 `.grpd`：
```bash
python3 Generate_py/generate_model.py Examples/Thermal_Validation_Steady_Steps/PD.yaml
```

### 2. 求解与日志监测
显式调用最新编译的 `GRPD` 可执行文件求解算例：
```bash
/Users/hanbozhang/C++/GRPD/bin/release/GRPD Examples/Thermal_Validation_Steady_Steps
```
重点观察收敛历史，检验在 Step 1 和 Step 2 的不同子步中，稳态隐式迭代是否能在极少迭代步内准确收敛，确认无 NaN 或异常发散。

### 3. Walkthrough 文档物理落盘
验证成功后，在 `docs/superpowers/plans/2026-06-17-add-steady-loadsteps-example-walkthrough.md` 物理落盘总结报告。
