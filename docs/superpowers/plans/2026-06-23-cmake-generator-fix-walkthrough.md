# Walkthrough - CMake 生成器与依赖项修复总结

本文件记录了解决 CMake 工具“找不到任何可用的生成器”报错以及 Eigen 依赖缺失问题的过程。

## 变更背景与问题

1. **生成器缺失报错**：
   在 Windows 系统下使用 TDM-GCC-64 编译器时，VS Code 的 CMake Tools 插件在未明确指定生成器时，默认会在系统 PATH 中寻找 `ninja`、`ninja-build` 或 `make`。由于用户只安装了 TDM-GCC，其附带的构建工具是 `mingw32-make.exe`，导致插件报出 “找不到任何可用的生成器” 错误，且 CMake 默认回退到 `NMake Makefiles` 时因缺少 `nmake` 报错。
2. **Eigen 依赖缺失**：
   项目拉取后，`Third/eigen` Git 子模块处于未初始化状态（目录为空，仅有 `.git` 指向文件），导致配置 MinGW 构建树时报出：
   `CMake Error at Third/CMakeLists.txt:29 (message): Eigen source not found in Third/eigen!`

## 解决方案与修改内容

### 1. 配置项目级 VS Code 设置
在项目根目录下新建 [settings.json](file:///c:/C++/GRPD/.vscode/settings.json)，显式指定 CMake 生成器为 `"MinGW Makefiles"`：
```json
{
    "cmake.generator": "MinGW Makefiles"
}
```
这强制让 VS Code 的 CMake Tools 插件使用 MinGW 生成器配合 TDM-GCC 编译链，并自动解析使用 `mingw32-make`。

### 2. 重新拉取并更新 Git 子模块
通过终端重置并完整拉取 `Third/eigen` 子模块内容：
```bash
git submodule deinit -f Third/eigen
git submodule update --init --recursive --progress
```

## 验证与测试结果

1. **CMake 配置验证**：
   运行 `cmake -G "MinGW Makefiles" -S . -B build` 配置成功：
   ```text
   -- OpenMP found and version is 4.5
   -- GRPD Top-Level Configuration Done!
   -- Build Type: Release
   -- Configuring done (10.6s)
   -- Generating done (0.5s)
   ```
2. **工程构建测试**：
   通过 `grpd-server` 的 `build_grpd` 工具进行 Release 构建，构建耗时 186.6 秒，编译出 `GRPD.exe` 静态库与所有测试文件，返回值为 `0` (Success)。
