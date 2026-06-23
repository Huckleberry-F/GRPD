# 任务列表 (task.md)

- [x] **Task 1: 修复 BC.h 中的不完整类型缺陷**
  - [x] 步骤 1：修改 `PDCommon/BC/include/BC.h`，将 `struct BCEntry` 移至 `class BC` 的定义后
  - [x] 步骤 2：使用 `build_grpd` 编译，验证有无 C++ 语法错误
- [x] **Task 2: 添加 CMakePresets.json 配置**
  - [x] 步骤 1：在项目根目录下新建 `CMakePresets.json` 并写入预设
  - [x] 步骤 2：测试 `cmake --preset win-ninja` 或 `cmake --preset win-mingw` 能否成功生成构建树
- [x] **Task 3: 添加一键部署脚本 setup_build.bat**
  - [x] 步骤 1：在项目根目录下创建 `setup_build.bat` 并写入一键初始化和编译逻辑
  - [x] 步骤 2：执行 `setup_build.bat`，验证是否能完整跑通环境检查、虚拟环境安装、依赖加载和基于 Preset 的自动编译
