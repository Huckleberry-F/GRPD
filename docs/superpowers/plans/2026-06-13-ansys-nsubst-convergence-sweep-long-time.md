# 扫描 ANSYS 子步数收敛性测试计划 (Time=1.0 & 2.0)

**Goal:**
在瞬态热传导 5x10 算例上，将物理时间放长至 $T=1.0\text{ s}$（第一载荷步）与 $T=2.0\text{ s}$（第二载荷步），保持 GRPD 步长 $dt = 1e-4\text{ s}$（共 20000 步）和 ANSYS 的 CN 隐式积分。扫描 ANSYS 的子步数 $NSUBST \in [100, 500, 1000, 2000, 5000]$。对标并验证 $t=2.0\text{ s}$ 时刻的温度解收敛情况。

## 实施步骤

### 1. 确认与定位 GRPD 结果
- 检查 `Examples/Thermal_Validation_5x10/output` 下最新的 `Result_20260613_*` 目录。
- 确认是否已成功生成 `Thermal_Validation_5x10_step20000_t2.0000.vtk`。
- 如果没有，则重新构建并运行 GRPD。

### 2. 生成 5 组 ANSYS APDL 宏文件
对于 $NSUBST \in [100, 500, 1000, 2000, 5000]$，分别生成宏文件：
- `ansys_smoke_test_n100.mac`
- `ansys_smoke_test_n500.mac`
- `ansys_smoke_test_n1000.mac`
- `ansys_smoke_test_n2000.mac`
- `ansys_smoke_test_n5000.mac`

APDL 宏的结构应为：
```apdl
/PREP7
! 几何与单元定义 (PLANE55)
RECTNG,0,5,0,10
ET,1,PLANE55
MP,DENS,1,7.85e-9
MP,KXX,1,45.0
MP,C,1,4.6e8
ESIZE,0.1
AMESH,ALL

! 边界条件
NSEL,S,LOC,X,0.0
SF,ALL,HFLUX,100.0
NSEL,S,LOC,X,5.0
D,ALL,TEMP,100.0
ALLSEL,ALL

/SOLU
ANTYPE,TRANS
TINTP,,0.5    ! Crank-Nicolson 隐式积分

! Load Step 1
TIME,1.0
NSUBST,N      ! 扫描的子步数
OUTRES,ALL,LAST
SOLVE

! Load Step 2
TIME,2.0
NSUBST,N      ! 扫描的子步数
OUTRES,ALL,LAST
SOLVE

! 数据输出
/POST1
SET,LAST
*CFOPEN,temp_profile_nN,txt
! 输出水平中线处的温度数据...
*CFCLOS
```

### 3. 执行 ANSYS 计算
- 调用 `ansys-server.run_ansys_mac` 运行这 5 组宏。
- 提取并保存各组的输出数据文件。

### 4. 路径插值对标
- 调用 `grpd-validation-server.compare_grpd_vtk_with_ansys_txt`，将 5 组 ANSYS 的温度曲线与 GRPD 结果进行一维路径插值对比，路径设为 $(0.0, 5.0, 0.0) \to (5.0, 5.0, 0.0)$。

### 5. 绘制收敛图与结果分析
- 编写 `scratch/analyze_convergence_long_time.py`，画出 5 组 ANSYS 温度与 GRPD 温度的对比曲线，并分析其收敛性质。
- 物理写入 `docs/superpowers/plans/2026-06-13-ansys-nsubst-convergence-sweep-long-time-walkthrough.md`。
