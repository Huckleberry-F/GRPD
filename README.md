# General Peridynamics 🚀

General Peridynamics (即原 GRPD) 是一个基于 **现代 C++ (C++17)** 编写的高性能、高扩展性近场动力学求解引擎。采用完全面向数据的设计和多态工厂架构，该引擎能够以极致的内存局部性和高度的模块化去处理多物理场（如各向同性热传导、动量方程）的非局部积分运算。其核心理论脱胎于 **非常规态基近场动力学 (NOSB-PD)**，为复杂域内的破裂与跨尺度问题提供强劲底层计算力。

---

## ✨ 核心特性与架构亮点

General Peridynamics 坚持极简的依赖项、严苛的性能优化以及面向工业级扩展的架构准则：

1. 🚀 **纯粹的面向数据设计 (Data-Oriented Design)**
   抛弃传统“大对象裹挟数据”的重度封装。所有物理量按需经由 `FieldManager` 转化为连续的 SoA（结构体数组转数组结构体）存储（即成百上千个长度为 `NUM_PARTICLES` 的 `double*` 一级指针）。对计算核直接暴露连续数组，完全消灭指针追逐 (Pointer Chasing)，榨干 L1 高速缓存。

2. 🧩 **高度解耦的多态隔离与注册中心 (Registry & Factory)**
   从底层的材料本构 (`Material`)、物理场清单 (`PhysicsFields`)，到边界解析器 (`MeshReader`)，全部采用单例工厂加编译期注册设计。只要新增一个对应类并调用注册宏，即可自动注入内核逻辑，无需对求解器本身进行任何硬编码修改，实现了功能“插拔”和极致解耦。

3. ⚡ **全面并发指令驱动 (Parallel Native)**
   利用 OpenMP 进行细粒度共享大数组并发调度。无论是巨量的距离和影响系数运算，还是状态张量的内部还原、断键惩罚以及积分方程的主循环，全链路铺满 `#pragma omp parallel for`，且无任何锁开销风险带来的竞争。

4. 📂 **泛用型多格式无缝对接工作流**
   从上游点云/网格到下游计算的入口已实现了标准化。内置 Python 工具集支持使用 Open3D 高速扫描构建含表面精修的百万级体素测试物，而内核 IO 则拥有解析自定义文本 (`.grpd`) 乃至经典分析网格 (`.inp`) 等多模态提取能力。

5. 🛡️ **表面体积修正与零能震荡抑制**
   内嵌高精度非常规态基模型的体积补偿计算，彻底解决材料表面粒子因积分域被截断带来的非物理“软化”与形状张量不可逆缺陷；同时集成了高效抑制点阵空间零阶伪能震荡的算法模块机制。

---

## 📸 效果演示

> _(提示: 您可以在此处插入使用 ParaView 渲染的热传导、裂纹扩展、应力云图等动画或高质量截图。一张直观的结果图将是吸引使用者的最好招牌)_

---

## 🚀 快速上手 (Quick Start)

让引擎跑起来只需简单几步！我们的计算管线通过 `PD.yaml` 灵活调度。

**1. 基本依赖准备**
*   **C++17 编译器**（需支持 OpenMP）：例如 TDM-GCC 或 Visual Studio 2019/2022+。
*   **CMake** (3.15+) 及 **Python** (3.10+)。Python 端依赖库请通过以下命令装载：
    ```bash
    pip install open3d numpy pyyaml pydantic
    ```

**2. 获取源码与编译**
务必使用 `--recurse-submodules` 拉取第三方子模块（如 yaml-cpp）：
```bash
git clone -b v1.2 --recurse-submodules https://github.com/Huckleberry-F/GRPD.git
cd GRPD
mkdir build && cd build
# 生成工程 (Windows 环境使用 VS 则可以直接 cmake ..)
cmake -G "MinGW Makefiles" ..   
# 执行最终多线程构建
cmake --build . --config Release -j 12
```

**3. 执行第一个算例**
程序编译生成至 `bin/release/` 下。让我们以自带的 Box 测试用例为例：
```bash
# 1. 移步业务运行目录
cd Examples\Box

# 2. 调用 Python 前处理脚本引擎，根据 YAML 将 STL 立体离散化生成初始节点网络
python ..\..\Generate_py\generate_model.py PD.yaml

# 3. 轰鸣起动机，调用 C++ 引擎（它会自动拾取本目录的 PD.yaml）
..\..\bin\release\GRPD.exe
```
计算结束后，当前目录下将自动生成形如 `Result_YYYYMMDD_HHMMSS/` 的时间戳数据包，直接向 ParaView 中一拉，即可探索分析您的模拟数据。

---

## 🏗️ 引擎工作流与多层体系深入概览

引擎的每一次“点火”，均按照以下 **6个核心层级** 依次激活与演化，每一层严格执行单一职责：

### 第 1 层：输入解析控制层 (IO & Parsing Gateway) 
这是外部世界与引擎握手的接驳点。
*   由 `IOManager` 自动寻获当前运行环境下的 `PD.yaml` 并转换为引擎参数结构。
*   调用 `ReaderRegistry` 获取当前环境最适配的 `MeshReader` 派生类。不管是点云结构还是 FEM 网格拓扑，统一翻译剥离并存储为高度扁平化、脱水的 `MeshData` 对象。

### 第 2 层：预处理与内存初始化池 (Initialization Pool)
从混沌进入有序状态。主导权交由 `PDEngineInitializer`：
*   由 `InitModel` 将 `MeshData` 的点数据转换为粒子抽象身份 (`ParticleManager`)。
*   由 `InitNeighbors` 计算邻域范围并压紧生成数以百万计的稀疏映射池 (`NeighborList`)，并一次性进行体积修复计算。至此，拓扑连接完成建立并固化。

### 第 3 层：大容量连续场接管层 (Core Data Fields)
整个底层的高性能命脉。
*   基于给定的 `Physics Type` 和材料种类，工厂会唤醒对应的注册器。随即，`FieldManager` 会向内存开辟大块的线性连续存储区 (即 `double*`)，作为 `Temperature`、`Volume`，乃至 `State Variables` 的最终宿主，为下一步的指针狂潮做准备。

### 第 4 层：时间积分与外力驱动器 (Time Integration & Drivers)
引擎大循环的节拍器。
*   这层主要承接 `TimeIntegrator` 显式或混合步进（例如 Explicit Euler, Velocity-Verlet）。
*   在每次循环起步时下发外力和边界条件约束锁 (`BCManager`)。

### 第 5 层：物理积分核方程体系 (Physics Integral Kernel)
真正的数学计算深渊：非常规态基 (`NOSB_Base`) 内部架构处理。
*   使用最高频的 OMP 并发大循环调取相邻空间相互作用系数。组装形状张量表观 $\mathbf{K}$ 并执行它的精确求逆；处理广义状态力和零能控制模式衰减惩罚并将其平铺。此层计算结果将回传直接影响粒子的运动状态更新。

### 第 6 层：力学和物理本构层 (Material Constitutive)
极微观尺度的裁决官。
*   如 `FourierThermalMat` (傅立叶热传导法则) 等均以无状态类的单例形态存在。`PDKernel` 积分时仅负责传参下发，材料层计算应力、热通量响应值并回传反馈。两者完全解耦，绝不互相知晓各自内存结构。

---

## 📁 目录结构与功能注解

```text
General-Peridynamics/
├── PDCommon/          # 核心底层通用架构与各类计算基底构件 (Core Base)
│   ├── BC/            # → 边界约束容器：位移锁定(DISP)/外力载荷(BODY_FORCE)/速度场控制
│   ├── Core/          # → 全局类型定义与内存配置抽象基类
│   ├── Damage/        # → 损伤力学架构：解耦的断裂模型组件与预制裂纹形态注册表
│   ├── Field/         # → 数据大动脉：连续内存场(SoA)生态与分配池
│   ├── IO/            # → 多态接驳口：结构化 YAML分析与多模态模型写入接口
│   ├── Kernel/        # → 物理内循环组件：广义力学(NOSB_M)与动态稳定器 (Stabilizer)
│   ├── Material/      # → 本构计算枢纽：弹性介质库(LinearElasticMat)与状态存储
│   ├── Model/         # → 模型构架图谱：承载网格转化信息的 ParticleManager
│   ├── Neighbor/      # → 邻域搜索引擎：CSR大容量索引建设及表面截断修正库
│   └── Utils/         # → 底层服务体系：日志打印系统、错误溯源捕手
├── Src/               # 主求解核心与全生命周期组装厂
│   ├── Engine/        # → 求解引擎多态池：定义Engine抽象机制与注册工厂
│   │   └── Solvers/   #   └── 具体的求解机实现仓库 (如 PDEngine 及特供力学初始化机制)
│   ├── Integration/   # → 时间演化推进器：分步差分演化大总管 (ExplicitEuler/CentralDifference)
│   └── main.cpp       # 引擎最高时序控制主点火程序入口
├── Generate_py/       # 辅助 Python 前处理与网格体素工厂
│   └── generate_model.py # → 使用 Open3D 构建高密度实体微元模型的瑞士军刀
├── Examples/          # 工程应用与测试基准全解靶场
│   ├── Box/           # → 均质长方体空间导热 Benchmark
│   ├── Sphere/        # → 标准球体径向扩展 Benchmark
│   ├── bone/          # → 特异性曲面网格 (如骨骼) 分析 Benchmark
│   └── Engine/        # → 复杂装配体级仿真案例
└── CMakeLists.txt     # 各平台编译一键流水线蓝本
```


---

## 📌 版本更新日志 (Changelog)

### v2.3 — 损伤断裂力学解耦与多材料扩展架构 (Damage Module Refactoring) (当前版本)

* **全局损伤机制解耦 (Damage Decoupling)**: 彻底重构 Peridynamics 断裂模块。将 `DamageModel` 的配置与判断逻辑由全局 `Solver` 级别剥离，下沉至 `Material` 本构层级。这种架构级的重大解耦使不同材料能够拥有独立自治的断裂失效法则，为未来处理多相复合材料复杂破坏、界面脱粘提供了坚实的扩展根基。
* **预制裂纹单例注册表 (PreCrack Registry)**: 引入 `PreCrackRegistry` 和 `PreCrack` 多态抽象体系，通过参数化几何描述（如 `QuadCrack` 四边形裂纹面）精确定位并自动切断初始拓扑连结。实现了在连续体素网格内无需前处理结构分割即可灵活定义三维内部裂纹。
* **状态变量内聚化管理**: 损伤状态等历史记忆变量完全作为一种内部场，依托标准 `allocateStateVariables` 机制由材料组件自我管理。断绝了物理积分内核与具体损伤演化的相互粘连，将内核演化系统还原为最纯粹的微分代数计算。

### v2.2 — 高性能零分配内核与多维惩罚架构升维

* **零内存分配生命周期 (Zero-Allocation Lifecycle)**: 彻底重构 `Stabilizer` 的基类体系与运行机制，引入强制性的 `preCompute` 生命期接口。将原本在时间积分主循环 (Hot-Loop) 中每步触发的属性下发、张量不变式拼装全盘剥离至初始化阶段。依靠预申请的高速大缓存（如 `pkKinvCache_`, `shapeAi_`），彻底根除了单步千万级的底层动态堆内存分配和 RTTI 检查。
* **物理维度自动降阶投影 (Auto Dimension Projection)**: Python 前处理工作流深度挂载 `Dimension` 控制选项。开启 2D 时将自动剥离冗余 Z 轴进行微元切片降维组装。巧妙利用数学恒等变换，让纯粹 3D 架构的 C++ 底层引擎在**零业务代码修改、无特判**的情境下，完成了纯平面的跨量级降维打击，粒子搜索开销几何级清空。
* **深度局部并发与 OMP Guided 均衡 (SIMD Vectorization & Load Balancing)**: 将力学 `NOSB_M` 和热学 `NOSB_T` 内核中涉及巨量点积计算的底循环通过 `#pragma omp simd` 强制挂载编译器矢量化寄存器，并手动实施不变式前置抽取。将所有物理场层级并发块挂入 `schedule(guided)` 调度网络，彻底歼灭表面和缺角粒子带来的多线程长尾饥饿问题。
* **现代 CMake 标准破壁前向兼容 (Forward Compatibility)**: 排查并修复了 `Eigen` 等下辖泛型模板库内陈旧的预处理语法。启用 `VERSION 3.14...4.1.0` 的闭区间宽恕声明，完美横跨对接最新 CMake 4.0.3+ 的所有规范性检验，全战线黄线报错清零。
* **零能控制模块角点异常解析 (Boundary Anomaly Resolving)**: 针对 Zhang 等刚度惩罚力算法在局部张量条件数病态断层时的“角点爆炸”痛点，给出了涵盖幽灵粒子 (Fictitious Nodes)、边界刚度衰减器 (Boundary Attenuation)、多态回退融合三种工业级化解纲领。

### v2.1 — 边界条件体系规范化与时间积分器统一重构

* **BC 分类体系统一**: 所有边界条件子类（热/力学共 7 个）全部显式声明 `isConstraint()` 覆盖，彻底消除依赖基类默认值的隐患。统一约定：Dirichlet 型（`set`, `isConstraint=true`）在积分步末尾重新施加；Neumann 型（`add`, `isConstraint=false`）仅在力计算前施加一次。
* **VelocityBC 叠加缺陷修复**: 修复了 `VelocityBC` 因缺少 `isConstraint()` 重写导致速度在每步不断累加的严重 bug。
* **力学 BC `apply()` 接口统一**: 全部力学边界条件的 `apply()` 方法从手动 `getData()` 下标操作重构为使用 `TypedField` 的 `set()`/`add()` 按分量接口，与热 BC 风格完全对齐。
* **时间积分器步骤结构规范化**: `ExplicitEuler` 与 `CentralDifference` 统一为「清零→源项→内力→积分→约束」的标准循环模板。`applySources()` 与 `applyConstraints()` 完全解耦，各自出现在逻辑正确的位置。
* **Python 前处理科学计数法容错**: 修复 `generate_model.py` 中因 PyYAML 将 `1.0e20` 解析为字符串导致的格式化崩溃。
* **通用算力流程封装**: 在 `TimeIntegrator` 基类中提取 `evaluateForces()` protected 函数，封装「清零率场→施加源项→计算内力」三步通用流程，消除子类间的重复代码。
* **力学位移加载算例**: 新增 `Examples/Box_Disp` 二维平面位移拉伸测试用例，配套速度驱动边界与线弹性本构。

### v2.0 — 非常规态基力学内核崛起与全域稳定化体系

* **大变形 NOSB 核心上趟**: 全面贯通了 `NOSB_M` 力学积分核。在双重 OMP 热点下实现了形变梯度 $\mathbf{F}$、广义形状算子 $\mathbf{K}^{-1}$、以及非仿射残存位移 $\mathbf{z}$ 的无分支萃取与高性能并发现发。
* **全系零能模式防护墙 (Zero-Energy Stabilizers)**: 构建了三套针对大变形切变与穿透的微观惩罚网络（Silling 标量法、Wan 纯四阶缩并法、Zhang 动态阻力张量阵）。通过对公式体系的终极重整，将原本极易拖垮缓存的全局阻滞张量替换为 L1 命中率爆表的 $O(N)$ 零长延时向量点积。
* **动量与显式差分驱动器**: 针对运动方程上线了 `CentralDifference` 核心差分推进引擎，被安全挂入全局 `TimeIntegrator` 网络，支持双物理场异阶时间分步演化。
* **固体微观材料工厂**: 完全封装了力学专有的线弹性本构 `LinearElasticMat` 基元，只需杨氏模量极简介入便可高效率投射拉梅常数反馈第一类皮奥拉-基尔霍夫 (PK1) 应力场。
* **精细化刚体边界**: 新增涵盖 `DISP`, `BODY_FORCE`, `VELOCITY`, `PRESSURE` 的四大结构动力学约束协议，真正实现由外部 YAML 安全指挥时域载荷施加。

### v1.5 — 稳态底层模块职责纯度与 Manager/Registry对齐

* **Field 模块统一工厂路线**: 销毁遗留的散装注册路径，统一由 `FieldRegistry` 接手 `DoubleField` 与 `IntField` 工厂分发，根除历史遗留。
* **三大容器模块多态闭环体系**: 抽象规范了 Field/Material/BC 实例化参数通道。

### v1.4 — 多态核心提炼与全面解耦
- **全局泛型工厂架构**: 将零能模式抑制项彻底从核积分体系中剥离，建立了跨物理场的 `Stabilizer` 纯虚抽象体系。
- **单例注册表注入**: 确立了 `StabilizerRegistry` 编译期加载机制，实现了无需硬编码的安全策略模式换血。
- **高性能二次型预计算**: 深度实装了 Zhang 的全维各向异性零能惩罚方法（基于逆向形状张量的 $\mathbf{K_T}$ 二次形式展开），并通过预计算场极大压缩了时间积分内循环压力。
- **同一 Part 多材料映射**: 前处理 `generate_model.py` 新增 `MatRegions` 特性，允许利用 AABB 几何分块安全切割并赋闲多材料 ID。

### v1.3 — 彻底纯化 IO 多态架构
- **剔除遗留硬编码**：完全删除了早期的粗暴读取类 `GrpdReader`。
- **解析逻辑闭环**：全格式多态解码均已融入 `GrpdMeshReader` 及新派生体系内。
- **提炼通用数据引擎**：实现了 `MeshData` 解耦数据泵，专门负责将外部点云拓扑无缝输送灌入底层物理场。

### v1.2 — 多态网格接入与 NOSB 表面补偿
- **自动 `MeshReader` 体系**：确立了基于文件后缀双向识别的多态工厂。
- **空间孤岛切除**：前置 Python 脚本引入了基于 Open3D 树扫描的筛选器，前置抹除了脱离主体大部队的游离死点。
- **NOSB 表面体积修正落地**：实现了基于实际邻域体积的修正补偿，抑制了表面截断引发的张量病态。

### v1.1 — 全局路径接管与无锁化迭代
- **智能工作目录**：`IOManager` 单例诞生，废除一切路径硬编码，程序开始基于本地 `PD.yaml` 机动执行。
- **全局并发矩阵**：极限线程发掘的大规模稀疏邻接映射池构建完成。

---

## 📅 未来研究方向与学术计划 (Roadmap)

当前的重构与开发已完成了 **底层架构极度提纯**，未来项目将沿着多物理耦合与防断裂机制的深水区全速推进。

### ✅ Phase 0. 稳态/瞬态纯热场算法闭环与零能抑制补测 (Completed)
- **热零能模式抑制 (Zero-Energy Mode Suppression) 多态实施**: 彻底完成了对点阵积分由于畸变/截面断裂所引发的虚假零能空间震荡进行多态化的补偿抑制。
- **基准标定与 ANSYS 仿真校验**: 热传导全解模型的数据结果已与顶级商软 ANSYS 在同平台网格下完成了深度映射校验，核心算法结果分毫不差。

### ✅ Phase 1. 结构大变形内核全覆盖推进实装 (Completed)
- **`MechanicalFields` 基础就绪**: 完美兼容且实装了力学位移、速度、加速度三维核心运动学场的底层注册化剥离。
- **`CentralDifference` 与多场协同**: 已实现且深度验证了具备高精度的二阶显式中心差分加速器以及异阶交错步积分编排 (`StaggeredIntegrator`)。
- **`NOSB_Mechanical` 与弹性力态根基落地**: 全量落实力学内核，彻底打破网格局限解算大形变梯度张量。实装针对各类穿透与沙漏变形的纯并发惩罚计算网络，杜绝任何零能崩溃点。
- **线弹性微观材料库搭建**: 植入可控的线弹性基元 (`LinearElasticMat`) 及其到宏观弹性 PK1 应力场的高速映射转化通道，现已完全具备纯净的介质抗拉防变能力。

### ✅ Phase 2. 高阶边界条件钳制与动态本领强化 (Completed)
- **力学专属边界条件全覆盖**: 完整实现了位移锁定 (`DisplacementBC`)、速度驱动 (`VelocityBC`)、体力载荷 (`BodyForceBC`)、压力加载 (`PressureBC`) 四大结构动力学约束协议，统一 Dirichlet/Neumann 分类与 `isConstraint` 显式声明体系。
- **时间积分器规范化**: `ExplicitEuler` 与 `CentralDifference` 统一为标准化步骤模板，BC 施加逻辑完全解耦且位置正确。
- **待后续扩展**: 动态加载场与时域查表加载机制 (Table Curve)、多物件刚体接触判定算法。

### 🌌 Phase 3. 破裂损伤行为与耦合大业
- **断键力学 (Damage & Failure)**: 给相邻键赋值临界伸长率，引入裂纹动态分叉与表面能量失效机制。彻底打开近场动力学处理非连续介质破坏的“杀手锏”功能。
- **大耦合体系 (`ThermoMechanical`)**: 开放跨架构的数据交互通道，计算由于瞬时温度梯度引发的内部热应力形变，以及因塑性剧变而逆向产生的激波与聚热。
- **强非线性降伏模块**: 开发包含 J2 等向强化效应等复杂塑性本构法则演算插件。

### ⚡ Phase 4. 高性能力量跃迁计算架构
- 向前探索基于 CUDA 的异构大规模并行核 `Gpu_PDKernel`，将局部积分加速率再次抬升量级尺度。探索与隐式时间迭代 `Implicit Integrator` 的底层联用设计。

> 👨‍💻 **项目哲学**: 计算科学的代码架构应如其推导的微分算式一样——**严密、纯粹、互不冗余且无处不展现高度的数学秩序**。这就是 General Peridynamics 永远的追求目标。
