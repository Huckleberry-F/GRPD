# GRPD (General Rectangular Peridynamics) 🚀

GRPD 是一个基于**现代 C++ (C++17)** 编写的高性能、高扩展性近场动力学 (Peridynamics) 求解引擎。目前主要实现了基于**非常规态基近场动力学 (NOSB-PD)** 的**各向同性热传导**求解。

本项目在架构设计上追求**极致的模块化与解耦**，核心模块全部遵循 **"接口驱动 + Registry (注册中心) + Factory (工厂) + Singleton (单例)"** 的工业级设计模式。

---

## 🏗️ 核心架构与多态体系

整个项目包含 **7 棵核心继承树**，它们通过编译期宏注册机制实现动态装配，新增物理模型、材料或边界条件**完全不需要修改引擎核心代码**。

### 1. 🔥 物理核 (`PDKernel`)
负责空间积分、形状张量计算与力态/热态计算。
- **基类**: `PDKernel`
- **NOSB 族底座**: `NOSB_Base` (提供形状张量的纯几何积分、影响函数计算、零能惩罚等通用支持)
- **当前实现**: `NOSB_T` (热传导内核)

### 2. 🧱 材料本构 (`Material`)
负责定义物理属性并生成所需的状态变量场。
- **基类**: `Material` -> `ThermalMaterial`
- **当前实现**: `FourierThermalMat` (傅里叶各向同性导热材料)

### 3. 🌡️ 边界条件 (`BC`)
管理各类边界的施加规则。
- **基类**: `BC` -> `ThermalBC`
- **当前实现**: `TemperatureBC` (Dirichlet/固定温度), `HeatFluxBC` (Neumann/热通量), `ConvectionBC` (Robin/对流换热)

### 4. 📊 物理场存储 (`Field`)
底层内存管理，采用 **SoA (Structure of Arrays)** 布局，通过 `FieldManager` 统一管理生命周期。
- **基类**: `Field`
- **当前实现**: `TypedField<T>` (模板化连续内存场，例如保存所有粒子的 `Coords`、`Temperature` 等，提取数据只需获取一次裸指针)

### 5. 🌌 场生成器 (`PhysicsFields`)
根据 YAML 配置的物理问题类型，自动向 `FieldManager` 批量注册所需的物理场。
- **基类**: `PhysicsFields`
- **当前实现**: `ThermalFields` (自动生成 Temperature, TempGradient, HeatFlux, TempRate), `MechanicalFields` 等。

### 6. ⏱️ 时间积分求解器 (`TimeIntegrator`)
- **基类**: `TimeIntegrator`
- **当前实现**: `ExplicitEuler` (显式欧拉差分)

### 7. ⚙️ 顶层引擎 (`Engine`)
- **基类**: `Engine`
- **当前实现**: `PDEngine` (统筹所有模块的初始化循环与步进计算)

---

## ⚡ 性能优化 (HPC)

由于 PD 算法计算量巨大，本项目在实现上实施了以下底层优化：

1. **OpenMP 级并行**: 所有的底层计算循环 (影响函数预计算、形状张量、边界条件施加、热量积分) 均基于 `#pragma omp parallel for` 进行多线程加速。
2. **极速的数据访问 (SoA)**: 抛弃传统的 `std::vector<Particle>` 肥大对象模式，所有关键物理量全部萃取至 `FieldManager` 中成为连续内存块。热循环通过原始裸指针 `double*` 访问以确保缓存命中！
3. **零动态开销**: 多态设计仅在初始化 (`configure/initialize`) 以及大阶段 (`computeForceState`) 调用虚函数。对于百万级别调用的极热路径函数 (`GetInfluenceWeight`, `ComputeZeroEnergyModePenalty`)，采用**内联 (inline) + 局部 switch**，促使编译器实现极限展开与自动 SIMD 向量化。
4. **影响函数与空间截断预计算**: $K^{-1}$ 和影响函数权重 $\omega$ 在初始化阶段合并计算并绑定到 `BondField`。且引入了基于距离阈值的邻域刚性截断过渡项，在无形中修剪边界误差的同时保证热循环零判断开销。

---

## 🛠️ 构建与运行

### 环境要求
- CMake 3.15+
- 现代 C++17 编译器 (GCC 9+, Clang, MSVC)
- OpenMP 支持
- 依赖项：Eigen3, yaml-cpp

### 编译指令
```bash
mkdir build && cd build
cmake ..
cmake --build . --config Release
```

### 运行
执行生成的 `GRPD.exe`。默认情况它会读取 `Input/PD.yaml` 中的配置文件并开始求解，并在 `Output/` 目录下生成用于 ParaView 查看的 VTK 文件。

---

## ✨ 近期更新说明 (v1 分支)

- **NOSB 架构重构**: 抽象出通用的 `NOSB_Base` 处理纯几何形状张量、零能修正与内核函数，并实现第一个派生类 `NOSB_T`。
- **多种零能模式修正式**: 在 `NOSB_Base` 框架中正式引入 Silling, Wan, Zhang 等多种零能纠正策略，由 YAML 枚举项动态指定，且使用加权体积进行精准修正。
- **性能修复**: 回退可能阻断 OMP 与 SIMD 分支预测的多态影响函数计算，将其压入内联函数体系中完美保证底层循环纯净。
- **废除冗余架构**: 由于 `FieldManager` 的完美接管，旧版用于分离 Particle 数据的 `DataExtractor` 获取方式已废弃，真正达成数据彻底解耦！