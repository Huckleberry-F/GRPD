# 实施总结: 蠕变本构模型开发 (Creep Constitutive Model)

## 核心成果

1.  **全局时间管理基础设施**:
    *   为 `PDContext` 添加了 `currentTime_` 和 `currentDt_` 属性以及相应的读写接口。
    *   对时间积分器（`ExplicitEuler`, `CentralDifference`, `ADR_Integrator`）进行了升级，确保在每次调用力学计算引擎前，将当前时间步精准同步至 `PDContext`。
    *   这一底层改造使得所有下游材料与本构模型都能原生、无缝地访问全局物理时间。

2.  **本构接口扩展**:
    *   扩展了 `MechanicalMaterial::ComputePK1Stress` 与 `ComputeEngineeringStress`，新增 `const PDContext* ctx = nullptr` 传参机制。
    *   同步升级了所有实体物理材料子类（`LinearElasticMat`, `J2PlasticityMat`, `JCPlasticityMat`, `J2VoceLemaitreMat`）的接口签名。
    *   在积分核求解器（`NOSB_M`, `NOSB_Axis`）以及测试用例（`test_constitutive`）中完成了相应的传参适配。

3.  **蠕变统一基类 (`CreepMaterialBase`)**:
    *   构建了继承自 `MechanicalMaterial` 的抽象基类，统一管理蠕变逻辑。
    *   开辟并管理了全新的内部状态变量场：等效蠕变应变 `EqCreepStrain`（标量）和蠕变应变张量 `CreepStrain`（张量），均配备双缓冲 (Old/Trial)。
    *   在 `ComputePK1Stress` 核心逻辑中，内置了基于 Prandtl-Reuss 流动法则的 **3D 张量径向返回映射 (Radial Return Mapping)**，彻底解决了偏应力方向更新难题。
    *   提炼了纯虚函数 `computeDeltaCreepStrain()`，作为供派生类接入 1D 蠕变经验律的标准化 API。

4.  **1D 经验律派生模型**:
    *   成功接入 **Norton-Bailey 蠕变** (`NortonBaileyCreepMat`): $\dot{\varepsilon}_{cr} = A q^n$
    *   成功接入 **时间硬化蠕变 (Time Hardening)** (`TimeHardeningCreepMat`): $\dot{\varepsilon}_{cr} = A q^n t^m$
    *   成功接入 **应变硬化蠕变 (Strain Hardening)** (`StrainHardeningCreepMat`): $\dot{\varepsilon}_{cr} = A q^n (\varepsilon_{cr})^m$
    *   所有模型均已完成 `MaterialRegistry` 的动态注册与参数解析挂载。

5.  **编译与工程验证**:
    *   通过修正 `PDCommon/Material/CMakeLists.txt` 的包含路径，成功解决了 `PDContext` 与 `MaterialManager` 之间复杂的头文件循环依赖。
    *   使用原生的 `TypedField` SoA 内存布局进行了状态变量管理验证。
    *   多线程编译通过，成功链接 `libPDCommon.a` 及 `test_constitutive.exe`。
