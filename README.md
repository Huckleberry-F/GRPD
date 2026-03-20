# GRPD (General Rectangular Peridynamics) 🚀

GRPD 是一个基于**现代 C++ (C++17)** 编写的高性能、高扩展性近场动力学 (Peridynamics) 求解引擎。目前主要实现了基于**非常规态基近场动力学 (NOSB-PD)** 的**各向同性热传导**求解。

本项目在架构设计上追求**极致的模块化与解耦**，核心模块全部遵循 **"接口驱动 + Registry (注册中心) + Factory (工厂) + Singleton (单例)"** 的工业级设计模式。

---

## 🏗️ 核心架构与多态体系

整个项目包含 **7 棵核心继承树**，它们通过编译期宏注册机制实现动态装配，从顶层引擎到底层计算模型新增任何功能**完全不需要修改控制流核心代码**。以下按照从宏观到微观 (自上而下) 的调用层级：

### 1. ⚙️ 顶层引擎 (`Engine`)

统筹所有模块的生命周期（初始化、循环、步进、导出）。

- **基类**: `Engine`
- **当前实现**: `PDEngine` (PD 求解专属引擎)

### 2. ⏱️ 时间积分求解器 (`TimeIntegrator`)

掌管时间步进循环与状态量的时间显式/隐式更新。

- **基类**: `TimeIntegrator`
- **当前实现**: `ExplicitEuler` (显式欧拉时间积分)

### 3. 🌌 场生成器 (`PhysicsFields`) & 📊 物理场存储 (`Field`)

**场生成器**负责根据物理问题"声明需要什么数据"，**物理场存储**负责"底层内存如何非结构化连续分配"。

- **基类**: `PhysicsFields` & `Field`
- **当前实现**: `ThermalFields` (自动生成 Temperature 等热流场)，配套使用 `TypedField<T>` 提供 SoA 热路径裸指针存储。

### 4. 🔥 物理核 (`PDKernel`)

执行内循环中最为密集的核心数学与物理计算（形状张量、影响函数权重、零能防范、状态演化散度项等）。

- **基类**: `PDKernel`
- **NOSB 族底座**: `NOSB_Base` (处理纯几何与稳定化支持)
- **当前实现**: `NOSB_T` (非常规态基热传导微分方程积分内核)

### 5. 🧱 材料本构 (`Material`)

只负责给物理核提供纯粹的物理属性系数与状态变量分配方案。

- **基类**: `Material` -> `ThermalMaterial`
- **当前实现**: `FourierThermalMat` (傅里叶各向同性材料)

### 6. 🌡️ 边界条件 (`BC`)

作为外循环修正项，在特定的节点边界上强行施加物理约束或额外源项。

- **基类**: `BC` -> `ThermalBC`
- **当前实现**: `TemperatureBC` (固定面温度), `HeatFluxBC` (指定热通量), `ConvectionBC` (牛顿冷却定律)

---

## ⚡ 性能优化 (HPC)

由于 PD 算法计算量巨大，本项目在实现上实施了以下底层优化：

1. **OpenMP 级并行**: 所有的底层计算循环 (影响函数预计算、形状张量、边界条件施加、热量积分) 均基于 `#pragma omp parallel for` 进行多线程加速。
2. **极速的数据访问 (SoA)**: 抛弃传统的 `std::vector<Particle>` 肥大对象模式，所有关键物理量全部萃取至 `FieldManager` 中成为连续内存块。热循环通过原始裸指针 `double*` 访问以确保缓存命中！
3. **零动态开销**: 多态设计仅在初始化 (`configure/initialize`) 以及大阶段 (`computeForceState`) 调用虚函数。对于百万级别调用的极热路径函数 (`GetInfluenceWeight`, `ComputeZeroEnergyModePenalty`)，采用**内联 (inline) + 局部 switch**，促使编译器实现极限展开与自动 SIMD 向量化。
4. **影响函数与空间截断预计算**: $K^{-1}$ 和影响函数权重 $\omega$ 在初始化阶段合并计算并绑定到 `BondField`。且引入了基于距离阈值的邻域刚性截断过渡项，在无形中修剪边界误差的同时保证热循环零判断开销。

---

## 🎯 当前实现功能 (Features)

目前项目主攻**热传导**问题，已实现完整的非常规态基功能闭环：

- **热本构模型**: 傅里叶各向同性热传导 (`FourierThermalMat`)。
- **丰富的边界条件**:
  - 第一类 (`TemperatureBC`): 恒定的固定温度。
  - 第二类 (`HeatFluxBC`): 给定热通量。
  - 第三类 (`ConvectionBC`): 对流热交换。
- **支持非均匀网格**: 底层积分完全基于粒子体积 (Volume) 与自动提取的局部特征尺寸 (dx)，无缝兼容局部网格加密与变大小时域/空域。
- **可选核函数 (Kernel)**: 支持 Constant, InverseDistance, Linear, Quadratic, Cubic, Quartic, Gaussian 共 7 种距离加权影响函数。
- **零能模式防范**: 实现了针对非常规态基数值震荡的零能惩罚修正 (Zero-Energy Mode Penalty)，目前默认支持基于加权体积积分的 Silling 修正方案。
- **输出格式**: 全面兼容 VTK 标准，直出支持 ParaView 各种高级体渲染与多场耦合过滤器的 `.vtu` 格式。

---

## 🚀 快速上手 (Setup & Trial)

### 1. 环境准备

本项目需要 CMake、C++17 编译器以及 Python 3.14（用于周边数据处理脚本，如需）：

- **安装 Python 3.14**:
  1. 访问 [Python 官方网站](https://www.python.org/downloads/)。
  2. 下载 Python 3.14 (目前可能为预发布版) 的 Windows 安装包。
  3. 运行安装程序，**务必勾选 "Add Python to PATH"**。
- **安装 Git**: 用于拉取代码库。
- **安装 C++ 工具链**:
  - **选项 A (推荐)**: TDM-GCC / MinGW-w64 (提供现代 GCC 编译器)。
  - **选项 B**: Visual Studio 2019/2022 (包含 MSVC 和 CMake)。

### 2. 克隆代码

打开终端 (Terminal) 或 PowerShell，使用 Git 克隆本项目的 `v1` 分支。**注意：本项目包含 yaml-cpp 和 eigen 子模块，必须添加 `--recurse-submodules` 参数**：

```bash
git clone -b v1 --recurse-submodules https://github.com/Huckleberry-F/GRPD.git
cd GRPD
```

*(如果克隆时忘记带参数，可在仓库拉取后运行 `git submodule update --init --recursive` 补充下载子模块)*

### 3. 配置与构建 (Build)

使用 CMake 进行项目的配置与编译。项目中已自带了用于数学计算的 Eigen3 库和解析 YAML 的 yaml-cpp 静态库。

```bash
# 1. 创建并进入构建目录
mkdir build
cd build

# 2. 生成构建系统文件
# -> 如果你使用的是 TDM-GCC (MinGW)，请指定生成器：
cmake -G "MinGW Makefiles" ..
# -> 如果你使用的是 MSVC (Visual Studio)，直接运行：
# cmake ..

# 3. 开始编译 (Release 模式获取最高性能)
cmake --build . --config Release
```

### 4. 试用流程 (Trial Run)

编译成功后，可执行文件会生成在 `bin/release/`（Windows）或对应目录下。

1. **修改配置文件**: 打开仓库根目录下的 `Input/PD.yaml`，你可以自由修改物理模型、材料参数 (如 `Density`, `ThermalConductivity`) 和网格尺寸 (`dx`)。
2. **运行求解器**:
   在 `build` 目录下运行：

   ```bash
   # Windows PowerShell
   ..\bin\release\GRPD.exe
   ```

3. **查看结果**:
   程序运行结束后，结果文件将输出在仓库的 `Output/` 目录下（通常为一系列 `.vtu` 格式的 VTK 文件）。
   - 下载并安装开源数据可视化软件 [ParaView](https://www.paraview.org/)。
   - 在 ParaView 中打开 `Output/` 目录下的 `.vtu` 文件，即可渲染和可视化动态的温度场或位移场结果！

---

## ✨ 近期更新说明 (v1 分支)

- **NOSB 架构重构**: 抽象出通用的 `NOSB_Base` 处理纯几何形状张量、零能修正与内核函数，并实现第一个派生类 `NOSB_T`。
- **多种零能模式修正式**: 在 `NOSB_Base` 框架中正式引入 Silling, Wan, Zhang 等多种零能纠正策略，由 YAML 枚举项动态指定，且使用加权体积进行精准修正。
- **性能修复**: 回退可能阻断 OMP 与 SIMD 分支预测的多态影响函数计算，将其压入内联函数体系中完美保证底层循环纯净。
- **废除冗余架构**: 由于 `FieldManager` 的完美接管，旧版用于分离 Particle 数据的 `DataExtractor` 获取方式已废弃，真正达成数据彻底解耦！
