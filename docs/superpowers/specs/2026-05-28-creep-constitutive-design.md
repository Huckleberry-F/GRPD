# 蠕变材料本构开发设计规范 (Creep Constitutive Models)

## 1. 目标与范围 (Goal & Scope)
在 GRPD 的非常规态基力学计算管线中，引入三种常用的蠕变本构模型：
1. **Norton-Bailey (二次蠕变)**：$\dot{\varepsilon}_{cr} = A q^n$
2. **Time Hardening (时间硬化)**：$\dot{\varepsilon}_{cr} = A q^n t^m$
3. **Strain Hardening (应变硬化)**：$\dot{\varepsilon}_{cr} = A q^n (\varepsilon_{cr})^m$

## 2. 架构方案 (Architecture)
采用 **基类统管 + 策略模式 (Approach 3)** 进行开发。

### 2.1 核心时间流形 (Time Tracking)
- **`PDContext` 更新**：新增 `double currentTime_` 字段及其 Setter/Getter。
- **积分器更新**：在 `ExplicitEuler`、`CentralDifference`、`MatrixFreeImplicitIntegrator`、`ADR_Integrator` 中，确保进入 `evaluateForces` 前调用 `ctx.setCurrentTime(currentTime);`。
- **接口扩展**：`MechanicalMaterial::ComputePK1Stress` 签名新增可选参数 `const PDCommon::Core::PDContext* ctx = nullptr`，并同步更新现有子类（`J2PlasticityMat` 等）。

### 2.2 本构类层级 (Class Hierarchy)
- **`CreepMaterialBase` (基类)**
  - 继承自 `MechanicalMaterial`。
  - 负责分配和绑定状态变量场：`EqCreepStrain_Old`/`Trial` (维数 1), `CreepStrain_Old`/`Trial` (维数 9)。
  - 内部实现统一的 3D 径向返回映射主算法 (Radial Return Mapping)。
  - 定义纯虚函数：`virtual double computeDeltaCreepStrain(double q_trial, double dt, double t, double eq_cr_old) const = 0;`。
- **`NortonBaileyCreepMat` (派生类)**
  - 重写纯虚函数，实现公式：$d\varepsilon_{cr} = A q_{trial}^n \cdot dt$。
- **`TimeHardeningCreepMat` (派生类)**
  - 重写纯虚函数，实现公式：$d\varepsilon_{cr} = A q_{trial}^n t^m \cdot dt$。
  - **异常处理**：若 $t=0$ 且 $m < 0$ 会导致除零奇异，代码内将默认截断 $t_{eff} = \max(t, 10^{-8})$。
- **`StrainHardeningCreepMat` (派生类)**
  - 重写纯虚函数，实现公式：$d\varepsilon_{cr} = A q_{trial}^n (\varepsilon_{cr})^m \cdot dt$。
  - **异常处理**：为避免初始蠕变应变为 0 导致的奇异，设定截断 $\varepsilon_{cr, eff} = \max(\varepsilon_{cr}, 10^{-8})$。

## 3. 数据流 (Data Flow)
1. Integrator 计算当前 $dt$ 与 $t$，存入 `PDContext`。
2. `NOSB_M` 将 `&ctx` 传递给材料的 `ComputePK1Stress`。
3. `CreepMaterialBase` 获取 $dt$ 与 $t$，调用试探态计算出 $q_{trial}$。
4. 多态调用派生类的 `computeDeltaCreepStrain` 获取 $\Delta \varepsilon_{cr}$。
5. 基类根据 Prandtl-Reuss 流动法则，将张量应力更新并回写至 Trial 物理场。

## 4. 验证计划 (Testing)
- 编译通过验证签名更新的安全性。
- （可选）使用一个含有蠕变材料的测试用例验证 $t$ 和 $dt$ 的传递是否正常，以及蠕变应变场是否成功分配。
