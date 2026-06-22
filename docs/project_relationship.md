# GRPD 项目关系图与架构说明

本文档基于 `codegraph` 的静态代码分析与检索结果，详细梳理了 GRPD (General Peridynamics Solver) 项目的架构设计与核心关系图。图表采用 Mermaid.js 格式绘制，主要展示了求解器的 **8层计算管线**、**核心类继承体系**、**PDContext 数据中枢**、**注册表与工厂单例模式** 以及 **模块间依赖方向**。

---

## 1. 8层计算管线执行流图

根据 `AGENTS.md` 的规范，GRPD 求解器在运行期间遵循严格的 8 层物理执行与内存隔离体系。以下是该计算管线的具体调用流向及各层对应的核心模块与类：

```mermaid
flowchart TD
    subgraph Layer1["层 1：输入解析控制层 (IO)"]
        A1[MeshReader / YAML 配置] -->|解析网格与作业参数| A2[IOManager]
    end

    subgraph Layer2["层 2：预处理与内存初始化池 (Topology)"]
        B1[ParticleManager] -->|初始化粒子与拓扑| B2[NeighborList]
    end

    subgraph Layer3["层 3：大容量连续场接管层 (SoA Fields)"]
        C1[FieldManager] -->|动态分配 SoA 内存连续物理场| C2[(双精度裸指针数组 double*)]
    end

    subgraph Layer4["层 4：时间积分与外力驱动器 (Integration)"]
        D1[TimeIntegrator] -->|驱动时间步推进| D2[BCManager]
        D2 -->|施加边界条件 BC| C1
    end

    subgraph Layer5["层 5：多体接触与摩擦防卫层 (Contact)"]
        E1[ContactManager] -->|运行排斥力与摩擦定律| E2[IContactAlgorithm / GeneralContact]
    end

    subgraph Layer6["层 6：物理积分核方程体系 (PD Kernels)"]
        F1[PDKernel / NOSB_Base / BB_Base] -->|计算非局部变形张量与沙漏稳定补偿| F2[Stabilizer / 零能稳定控制]
    end

    subgraph Layer7["层 7：力学和物理本构层 (Materials)"]
        G1[Material / MechanicalMaterial / ThermalMaterial] -->|计算应力与热通量 / 更新状态变量| G2[State Variables / 历史数据落盘]
    end

    subgraph Layer8["层 8：损伤演化与拓扑断裂层 (Fracture)"]
        H1[DamageModel / FractureModel] -->|评估断键阈值| H2[更新近邻拓扑断键]
    end

    %% 执行流向
    Layer1 --> Layer2
    Layer2 --> Layer3
    Layer3 --> Layer4
    Layer4 --> Layer5
    Layer5 --> Layer6
    Layer6 --> Layer7
    Layer7 --> Layer8

    classDef layerFill fill:#f9f,stroke:#333,stroke-width:2px;
    class Layer1,Layer2,Layer3,Layer4,Layer5,Layer6,Layer7,Layer8 layerFill;
```

---

## 2. 核心类继承体系图

GRPD 基于 C++17 构建，大量使用面向对象的多态机制来实现求解器各组件的灵活插拔。以下是核心接口与其具体派生类的继承树：

```mermaid
classDiagram
    %% PDKernel 体系
    class PDKernel {
        <<Abstract>>
        +computeForces() void*
        +initialize() void*
    }
    class BB_Base {
        <<Abstract>>
        comment: 键基近场动力学基类
    }
    class NOSB_Base {
        <<Abstract>>
        comment: 非常规态基近场动力学基类
    }
    PDKernel <|-- BB_Base
    PDKernel <|-- NOSB_Base
    NOSB_Base <|-- NOSB_M : 力学态基
    NOSB_Base <|-- NOSB_T : 热学态基
    NOSB_Base <|-- NOSB_Axis : 轴对称态基

    %% Material 本构体系
    class Material {
        <<Abstract>>
        +allocateStateVariables(FieldManager&) void*
        +updateMaterialResponse() void*
    }
    class MechanicalMaterial {
        <<Abstract>>
    }
    class ThermalMaterial {
        <<Abstract>>
    }
    Material <|-- MechanicalMaterial
    Material <|-- ThermalMaterial
    MechanicalMaterial <|-- LinearElasticMat : 线性弹性
    MechanicalMaterial <|-- J2PlasticityMat : J2 塑性
    MechanicalMaterial <|-- J2VoceLemaitreMat : 隐式 Lemaitre 塑性损伤
    MechanicalMaterial <|-- CreepMaterialBase : 蠕变基类
    ThermalMaterial <|-- FourierThermalMat : 傅里叶热传导

    %% TimeIntegrator 时间积分器体系
    class TimeIntegrator {
        <<Abstract>>
        +integrateStep() void*
    }
    TimeIntegrator <|-- ExplicitEuler : 显式欧拉
    TimeIntegrator <|-- CentralDifference : 中心差分
    TimeIntegrator <|-- ADR_Integrator : 自适应动态弛豫
    TimeIntegrator <|-- StaggeredIntegrator : 温度-力学交替积分

    %% BC 边界条件体系
    class BC {
        <<Abstract>>
        +apply(double time) void*
    }
    class MechanicalBC {
        <<Abstract>>
    }
    class ThermalBC {
        <<Abstract>>
    }
    BC <|-- MechanicalBC
    BC <|-- ThermalBC
    MechanicalBC <|-- DisplacementBC : 位移约束
    MechanicalBC <|-- BodyForceBC : 体力载荷
    MechanicalBC <|-- VelocityBC : 速度载荷
    ThermalBC <|-- TemperatureBC : 温度约束
    ThermalBC <|-- HeatFluxBC : 热通量载荷
    ThermalBC <|-- ConvectionBC : 对流边界

    %% FractureModel 损伤断裂体系
    class FractureModel {
        <<Abstract>>
        +evaluateBondStatus() void*
    }
    FractureModel <|-- BondStretchFracture : 键伸长量阈值
    FractureModel <|-- EqPSFracture : 等效塑性应变阈值
    FractureModel <|-- DamageValueFracture : 状态量损伤值阈值

    %% MeshReader 读网格体系
    class MeshReader {
        <<Abstract>>
        +readMesh(string path) MeshData*
    }
    MeshReader <|-- GrpdMeshReader : GRPD 原生格式
    MeshReader <|-- InpMeshReader : ABAQUS INP格式
```

---

## 3. PDContext 核心数据中枢关系图

`PDContext` 是 GRPD 的核心数据总线，它持有计算所需的所有管理器 (Managers)、场 (Fields) 和拓扑拓扑关系。所有模块通过传递 `PDContext` 的引用来实现解耦协作，而不需要全局直接依赖求解器核心。

```mermaid
classDiagram
    class PDEngine {
        -context : PDContext
        -integrator : unique_ptr~TimeIntegrator~
        -kernels : vector~unique_ptr~PDKernel~~
        +initialize() void
        +solve() void
    }

    class PDContext {
        -particleManager : unique_ptr~ParticleManager~
        -materialManager : unique_ptr~MaterialManager~
        -fieldManager : unique_ptr~FieldManager~
        -neighborList : unique_ptr~NeighborList~
        -bcManager : unique_ptr~BCManager~
        -contactManager : unique_ptr~ContactManager~
        -postProcessorManager : unique_ptr~PostProcessorManager~
        +getParticleManager() ParticleManager&
        +getFieldManager() FieldManager&
        +getMaterialManager() MaterialManager&
        +getNeighborList() NeighborList&
        +getBCManager() BCManager&
    }

    PDEngine *-- PDContext : 持有并驱动
    PDContext *-- ParticleManager : 粒子拓扑
    PDContext *-- MaterialManager : 材料本构管理
    PDContext *-- FieldManager : SoA 物理场内存管理
    PDContext *-- NeighborList : 邻域近邻链表
    PDContext *-- BCManager : 边界条件管理器
    PDContext *-- ContactManager : 接触算法管理器
    PDContext *-- PostProcessorManager : 后处理输出 vtk
```

---

## 4. 注册表与工厂单例模式

GRPD 使用了编译期静态注册与反射机制。每新增一个材料、算法或内核，只需通过对应的宏注册到各自的 `Registry` 中。`Manager` 会通过字符串标识从 `Registry` 动态实例化具体子类。

```mermaid
classDiagram
    %% 单例注册表与工厂通用设计模式
    class BaseModule {
        <<Interface>>
    }
    
    class ModuleRegistry {
        -registryMap : map~string, CreatorFunc~
        +getInstance() static ModuleRegistry&
        +registerClass(string typeName, CreatorFunc creator) void
        +create(string typeName) unique_ptr~BaseModule~
    }

    class ModuleManager {
        -instances : vector~unique_ptr~BaseModule~~
        +createAndAddInstance(string typeName) void
    }

    ModuleManager --> ModuleRegistry : 调用 create 创建实例
    ModuleRegistry --> BaseModule : 实例化具体派生类

    note for ModuleRegistry "通过宏 REGISTER_CLASS(TypeName, Class) \n在编译期静态向 registryMap 注册创建函数"
```

项目中具体包含以下注册表单例（均遵循上述设计模式）：
- `EngineRegistry` (负责求解引擎的选择，如 `"PD"`)
- `KernelRegistry` (负责物理积分核，如 `"NOSB_M"`, `"BB_Base"`)
- `MaterialRegistry` (负责材料本构选择，如 `"LinearElastic"`)
- `TimeIntegratorRegistry` (负责时间积分选择，如 `"ADR"`)
- `BCRegistry` (负责边界条件分配)
- `FieldRegistry` / `PhysicsFieldRegistry` (负责物理场及状态变量生成)
- `ContactRegistry` (负责接触与排斥算法选择)
- `StabilizerRegistry` (负责零能控制稳定器，如 `"Zhang"`, `"Wan"`)
- `FractureRegistry` (负责损伤断裂模型选择)
- `ReaderRegistry` (负责网格解析器分配)

---

## 5. 模块间依赖方向图 (PDCommon 拓扑)

为了保持求解器的结构清晰，`PDCommon` 内部模块均保持了高度的解耦。其依赖关系呈现自上而下的单向拓扑结构，严禁反向强耦合：

```mermaid
flowchart TD
    Src[Src/ 求解核心引擎与 main 入口] --> Core[PDCommon/Core - PDContext 及核心基类]
    
    Src --> Integration[Src/Integration - 时间积分器]
    Integration --> Core
    
    Core --> Field[PDCommon/Field - SoA 物理场管理器]
    Core --> IO[PDCommon/IO - 网格及参数解析]
    Core --> Neighbor[PDCommon/Neighbor - 拓扑近邻关系]
    
    Src --> Kernel[PDCommon/Kernel - PD 积分物理核]
    Kernel --> Core
    
    Src --> Material[PDCommon/Material - 本构响应]
    Material --> Field
    
    Src --> BC[PDCommon/BC - 载荷与约束]
    BC --> Field
    
    Src --> Contact[PDCommon/Contact - 排斥与摩擦]
    Contact --> Core
    
    Src --> Fracture[PDCommon/Fracture - 损伤裂纹演化]
    Fracture --> Neighbor
    
    Src --> PostProcessing[PDCommon/PostProcessing - VTK 输出]
    PostProcessing --> Field
    
    %% 通用工具
    Core -.-> Utils[PDCommon/Utils - 数学与工具函数]
    Kernel -.-> Utils
    Material -.-> Utils
```
