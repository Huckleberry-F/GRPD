# README 当前主线同步 Walkthrough

## 任务目标

根据当前 `main` 分支代码、近期提交历史、构建脚本、MCP 服务和示例算例，更新 `README.md`，让项目首页反映 GRPD 近期已经落地的工程能力。

## 事实来源

- `git log --oneline -n 40`：确认近期提交集中在 Windows 开发体验、MCP/验证服务、MatrixFreeImplicit 热稳态求解、VTK 输出、跨平台脚本、蠕变本构和仓库清理。
- `Src/Integration/src/MatrixFreeImplicitIntegrator.cpp`：确认隐式求解器支持一阶热传导目标、二阶力学目标、JFNK/L-BFGS、边界残差归零、多载荷步输出和收敛后状态提交。
- `PDCommon/Material/src/CreepMaterialBase.cpp` 与三类蠕变材料实现：确认 `EqCreepStrain_Old/Trial`、`CreepStrain_Old/Trial` 状态变量、径向返回和材料注册链路。
- `.agents/settings.json.example` 与 `.agents/setup_mcp.py`：确认五类 MCP 服务和跨平台自动配置路径。
- `setup/win`、`setup/mac`、`setup/linux` 与 `CMakePresets.json`：确认当前构建入口和 Windows/macOS preset。
- `PDCommon/IO/src/IOManager.cpp`：确认 Legacy `.vtk` 时间步输出会维护 `.vtk.series` 索引文件。

## 已完成修改

- 在 `README.md` 核心特性中补充：
  - `MatrixFreeImplicitIntegrator` 当前支持的二阶力学平衡与一阶稳态热传导残差。
  - `CreepMaterialBase` 和三类蠕变材料。
  - `.agents/` 下的 SuperCAE 技能与 MCP 验证工具链。
- 更新快速上手：
  - 移除旧的 `v4.5` 克隆命令和根目录旧脚本描述。
  - 改为 `setup/win`、`setup/mac`、`setup/linux` 平台脚本。
  - 增加 `CMakePresets.json` 手动构建命令。
  - 说明 `.vtk.series` ParaView 时间序列入口。
- 更新静态能力矩阵：
  - 补入 `MatrixFreeImplicit`、`J2VoceLemaitre`、三类蠕变材料和五类 MCP 服务。
- 更新目录结构：
  - 补入 `.agents/`、`setup/`、`CMakePresets.json`。
  - 将旧的 `Damage/` 修正为当前 `Fracture/`。
  - 增加 `PostProcessing/` 与热验证示例目录。
- 更新项目规模：
  - 按 `Src`、`PDCommon`、`Examples`、`Generate_py` 下 C++/Python 源文件重新统计。
- 在 Changelog 顶部新增 `v5.0`：
  - 汇总稳态隐式热、JFNK/L-BFGS 输出对齐、载荷因子修复、蠕变本构、MCP 服务、ANSYS/GRPD 对标增强、`.vtk.series` 输出、跨平台开发体验和仓库清理。

## GRPD 风险字段

**Touched Pipeline Layers:** 文档同步覆盖层 1 输入与输出、层 3 FieldManager 状态变量、层 4 TimeIntegrator、层 6 Kernel、层 7 Material、验证与构建工具链说明；未修改 C++ 执行逻辑。

**Domain Skills:** `grpd-cae-router`、`material-model-implementer`、`solver-loop-safety-reviewer`、`postprocess-exporter`、`cmake-build-doctor`。

**State Risk:** 无代码状态写入改动；README 仅描述现有 Old/Trial/Commit 链路。

**SoA Risk:** 无字段分配逻辑改动；README 仅描述 `FieldManager` 现有状态变量链路。

**Parallel Risk:** 无 OpenMP 逻辑改动。

**Validation:** 文档级检查包括 README 关键字扫描、git diff 审阅、近期提交与代码事实交叉核对；未运行 GRPD 数值算例。

## 后续建议

- 后续若继续新增求解器或材料模型，应在同一次提交中同步更新 `README.md` 的能力矩阵和 `docs/GRPD_YAML_Dictionary.md`。
- 若要把 README 作为对外发布首页，建议再补一节“当前推荐算例”，把 `Thermal_Validation_Steady_Steps`、`Thermal_Validation_5x10`、`Axisymmetric_Ring` 的运行目的和预期输出列清楚。
