# GRPD 引擎核心 YAML 字段速查字典 (终极全量版)

本文档通过深扒当前 GRPD C++ 项目源码库（包含 `InitXX.cpp`、各种接触及材料类的底层解析函数），提取了**所有存在于你当前代码逻辑中的实际读取字段**。这是 GRPD 目前能听懂的所有命令。

所有的 YAML 配置文件应严格绝不混乱地遵循以下 **6 大顺序区块**进行编写：

---

## 1. 模型建模 (System & Parts)

### 1.1 全局系统设置

由底层预处理直接提取的基本信息：

```yaml
System:
  Dimension: 2 # [必填] 2D / 3D
  SavePath: "output" # [选填] 后处理默认输出绝对或相对路径

Assembly: {} # 装配体节点占位，留作层级管理
```

### 1.2 几何部件生成与定位 (Parts)

由 `generate_model.py` 前处理脚本读取的全部支持字段：

```yaml
Parts:
  - PartID: 1 # [必填] 零件全局唯一标识
    MatID: 1 # [必填] 分配的材质 ID
    Dimension: 2 # [必填] 跟随全局通常为 2/3
    Shape: "Box" # [选填] 预设形状: "Box", "Circle", "Sphere"
    Length: 100.0 # [Box 专属] X 向长
    Width: 40.0 # [Box 专属] Y 向宽
    Thickness: 1.0 # [Box/Circle 2D专属] Z 向厚度，不填默认为 1.0
    Radius: 10.0 # [Circle/Sphere 专属] 半径
    Source: "mesh.stl" # [外部几何专属] 引用的 STL 网格路径
    dx: 0.5 # [必填] 粒子切分分辨率
    Offset: [0.0, 0, 0] # [选填] XYZ 平移（同 Translate）
    Translate: [0, 0, 0] # [选填] XYZ 平移（常用于 STL）
    Rotate: [0, 90.0, 0] # [选填] XYZ 欧拉角旋转（度）
    Scale: 1.0 # [选填] STL 缩放倍率
    RepairMesh: false # [选填] 是否调用 Pymeshfix 修复水密性
    MatRegions: # [选填] 单零件多材料分区映射
      - Box: [-1, 1, 0, 5, 0, 1]
        MatID: 2
```

---

## 2. 材料与断裂赋予 (Materials & PreCracks)

### 2.1 预裂纹 (PreCracks)

由 `InitPreCracks.cpp` 读取。

```yaml
PreCracks:
  - CrackID: 1
    Type: "QuadCrack" # [当前唯一可用] 空间四边形切割面
    V1: [-1.0, 75.0, -10.0] # 顺时针或逆时针顶点 1-4
    V2: [-1.0, 75.0, 10.0]
    V3: [50.0, 75.0, 10.0]
    V4: [50.0, 75.0, -10.0]
```

### 2.2 材料体系 (Materials / Material 标签均可识别)

不同子材料类读取的专属字段。

```yaml
Materials:
  # ==================================
  # [线弹性] 源自 LinearElasticMat.cpp
  # ==================================
  - MatID: 1
    Name: "Base_Steel"
    Type: "LinearElasticMat"
    Density: 7.8e-9 # [共用] 密度
    YoungsModulus: 2.1e5 # [必填] 杨氏模量
    PoissonsRatio: 0.3 # [必填] 泊松比

  # ==================================
  # [J2 弹塑性] 源自 J2PlasticityMat.cpp
  # ==================================
  - MatID: 2
    Type: "J2PlasticityMat"
    Density: 7.8e-9
    YoungsModulus: 2.1e5
    PoissonsRatio: 0.3
    YieldStress: 350.0 # 初始屈服极限
    HardeningModulus: 1000.0 # 硬化模量
    HardeningType: "Isotropic" # "Isotropic" (各向同性) 或 "Kinematic" (随动)
    LargeDeformation: false # 是否启用大变形构型更新

  # ==================================
  # [JC 毁伤弹塑性] 源自 JCPlasticityMat.cpp
  # ==================================
  - MatID: 3
    Type: "JCPlasticityMat"
    # 前序包含所有 J2 参数，附加专属损伤常量：
    JC_D1: 0.05
    JC_D2: 3.44
    JC_D3: -2.12
    # 缺失 D4/D5 说明目前 C++ 底层没有加应变率效应及温度效应

  # ==================================
  # [热传导材料] 源自 FourierThermalMat.cpp
  # ==================================
  - MatID: 4
    Type: "FourierThermalMat"
    Density: 7.8e-9
    Conductivity: 45.0 # k
    HeatCapacity: 460.0 # Cp

    # ==================================
    # [统一断裂] 源自 Material.cpp 基础类
    # ==================================
    Fracture:
      Type: "DamageValueFracture" # 或 "BondStretchFracture"
      Critical_Damage: 0.99 # [DamageValueFracture 专属]
      Critical_Stretch: 0.02 # [BondStretchFracture 专属]
```

---

## 3. 边界施加 (BoundaryConditions)

原生字典支持两种读取模式。被 Python 脚本当层解析剥离为 `.grpd` 的 `*LOAD` 块。
单点控制字典：

```yaml
BoundaryConditions:
  - Step: 0 # 生效关联的加载步（与 Solver.LoadSteps 挂钩）
    BcID: 1
    Name: "Grip_Bottom"
    PartID: 1 # [选填] 指定部件过滤
    Box: [-999, 999, -1, 3, -1, 1] # [必填] 空间包围盒过滤机制
    Type: "UX" # 支持 ["UX","UY","UZ", "VX","VY","VZ", "AX","AY","AZ", "PX","PY","PZ", "DISP", "VELOCITY"]
    Value: 0.0 # 单自由度时为标量，DISP/VELOCITY 复合自由度时填数组 [0.0, 0.0, 0.0]
    ApplyDir: [true, false, false] # [选填复合专用] 选择锁定哪些轴的转换
```

_(同时支持 `LoadStep_Conditions` 下级嵌套列表格式，效用一致，由 Python 完全兼容。)_

---

## 4. 接触对设置 (ContactPairs)

如果遇到错误，也可兼容被包裹在 `Solver: { ContactPairs: [...] }` 层级下（`InitContact.cpp`第17\~20行揭示）。

```yaml
ContactPairs:
  - Name: "Pair1"
    Type: "General" # 统一接触基类 (在v4.4扁平化后)
    Evaluator: "NTS" # 探测与几何评价器: "NTN" (节点对节点), "NTS" (节点对面), "NTC" (节点对群组)
    ForceLaw: "KinematicNormal" # 力学本构法则: "KinematicNormal", "Penalty", "Nonlinear", "Silling", "Viscous"
    FrictionLaw: "TanhRegularized" # 摩擦法则设定 (可选): "TanhRegularized", "ElasticStickSlip", "ExplicitImpulse"
    MasterPartID: 2 # [必填]
    SlavePartID: 1 # [必填]

    # 常规共同容差参数
    PinballRatio: 1.1 # 接触半径/容差识别膨胀倍率 (NTN时主导斥力触发距离，NTS时膨胀包络面)
    RestitutionCoeff: 0.0 # 动能恢复（反弹）系数

    # --------------------------------
    # 对应 KinematicNormal / ExplicitImpulse (动力学动量映射/修正)
    # --------------------------------
    # 几乎无需调整其他参数设置。它基于运动学投影进行直接速度干预。

    # --------------------------------
    # 对应 Penalty 类 (如 Penalty, Viscous)
    # --------------------------------
    PenaltyFactor: 1.0 # 根据体积模量算出的刚度乘子
    PenaltyStiffness: 1.0e6 # [覆写级] 直接写死弹簧刚度值
    DampingCoeff: 0.5 # 切向与法向共同阻尼系数

    # --------------------------------
    # 对应 Nonlinear (非线性硬化)
    # --------------------------------
    NonlinearOnsetRatio: 0.2 # 深层穿透的触发临界深度比 (0~1)
    StiffeningCoeff: 10.0 # 进入临界后刚度按指数放大的烈度

    # --------------------------------
    # 对应 Silling (PD标准互斥点力)
    # --------------------------------
    StiffnessFactor: 1.0 # Silling 短程力的倍率 c_s 标定倍率
    ShortRangeStiffness: 1.0e7 # [覆写级] 直接写死 c_s 参数
```

---

## 5. 求解积分器选择 (Solver)

整个底层系统大管家（涉及 `EngineManager.cpp`、`InitSolverComponents.cpp` 及所有积分器）。

### 5.1 引擎与内核通用项

```yaml
Solver:
  Engine: "PD" # 必须设定为 "PD"，不然引擎直接丢弃
  Type: "Mechanical" # "Mechanical", "Thermal", "ThermoMechanical"
  TimeIntegrator: "ADR" # "CentralDifference", "ExplicitEuler", "ADR"

  # --- 核函数组合支持数组注入法 ---
  Kernels:
    - "NOSB_M"
    - "BondBased"
  # --- 或者传统单体注入法 ---
  Kernel: "NOSB_M"

  # --- 全局常驻变量 ---
  Horizon: 1.5075 # 近场作用视界半径
  MassScaleFactor: 1.0e5 # 显式积分伪缩放质量（非常重要）
  OMP_Threads: 16 # 并发数
  OutputInterval: 1000 # 存档帧的间隔次数

  # --- NOSB_M Kernel 专属参数 ---
  DynamicShapeTensor: false # 是否动态更新变形配置矩阵（防奇点设为 false）
  KernelWeightType: "Linear" # 核函数锥度："Linear", "Gaussian", "Constant"
  ZeroEnergyG0: 1.0 # 零能镇定刚度常量
  ZeroEnergyMethod: "Zhang" # 沙漏惩罚模式："Zhang", "Silling"
```

### 5.2 阶段加载控制 (LoadSteps)

所有基于时间积分的算法必须配置这套逻辑！

```yaml
LoadSteps:
  - Step: 1
    Time: 1.0 # 持续时间或结束时间标记
    TimeStep_dt: 1.0e-5 # [显式积分必备] 当前极小的运行步长
    KBC: 0 # 0=斜坡加载缓施，1=恒定阶跃激增
    NumSubsteps: 10 # 将大位移分割进行的折算（多用于准静态平滑）
```

### 5.3 ADR 动松弛算子专属设置 (ADR_Integrator.cpp)

当 `TimeIntegrator: "ADR"` 启用后，这套非线性准静态监控网触发。

```yaml
  # --- 终止检查 ---
  MaxPseudoSteps: 15000         # 每级位移加载最大防死循环虚步数
  DispTol: 1.0e-5               # 基于增量位移矩阵 L2 范数的收敛容差
  ForceTol: 1.0e-3              # 基于不平衡力比值的收敛容差
  MinRefForce: 1.0e-2           # 用于防止全局受力为0导致 ForceTol 分母爆炸的保险常数

  # --- 松弛过程行为 ---
  RampIters: 0                  # 爬坡迭代虚步：写 0 意味着全权由阻尼全自动推算
  RampWaveRatio: 3.0            # 全自动推算时的波速基准比系数，越大启动越不冲
  DampingMethod: "HybridKinetic"# 阻尼消耗方式: "HybridKinetic", "Dynamic", 默认为静态阻尼
  Fracture_Strategy: "Strict"   # "Strict", "Staggered" 判断应变达到极限后是否延迟阻断以防突变
```

---

## 6. IO 输出设置 (Writer)

数据写入模块 `PDEnginePost.cpp`：

```yaml
Writer:
  Name: "CustomPrefix"           # [选填] 取代输出文件的默认模型名
  Type: "binary"                 # "binary" 二进制高压格式，或 "ascii" 会极度拖慢速度
  Variables:                     # 自由提取希望输出的底层场
    - Name: "PartID"
      Dimension: 1
    - Name: "Displacement"
      Dimension: 3
    - Name: "Velocity"
      Dimension: 3
    - Name: "Acceleration"
      Dimension: 3
    - Name: "PK1Stress"          # 第1类皮奥拉-基尔霍夫应力
      Dimension: 9
    - Name: "CauchyStress"       # 柯西应力 (部分内核下提供)
      Dimension: 9
    - Name: "DeformationGradient"# 变形偏梯度
      Dimension: 9
    - Name: "ShapeTensorInv"     # 反向形状张量（用于排查零能爆破点）
      Dimension: 9
    - Name: "EqPlasticStrain_Old"# 有效累积塑性应变
      Dimension: 1
    - Name: "VonMisesStress"
      Dimension: 1
    - Name: "Damage_Trial"
      Dimension: 1
    - Name: "BondIntegrity"      # 剩余完整键比例 0.0~1.0
      Dimension: 1
    - Name: "ActiveStatus"       # 存活标志位
      Dimension: 1
    - Name: "IsSurface"
      Dimension: 1
```
