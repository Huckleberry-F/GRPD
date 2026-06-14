# Multiplatform Setup and Build Walkthrough

## 变更概述

为了实现真正的跨平台环境初始化、编译器与依赖检查、MCP 服务注册以及 C++ 引擎的编译构建，我们将原本杂乱分散的脚本进行了平台级物理隔离与模块化拆分。

1. **新建 `setup/` 物理目录**：
   - 按照 `win/`、`mac/`、`linux/` 划分不同的平台子目录，逻辑高度解耦。
2. **三段式原生脚本设计**：
   - `check_env`：负责环境只读检测与版本汇报。
   - `install_deps`：负责补充系统工具链，创建 `.venv`，升级并安装 `requirements.txt` 及 MCP 必要包，并调用 `.agents/setup_mcp.py` 自动化注册 IDE 级 MCP 配置文件。
   - `build_project`：负责 C++ 引擎的多线程编译及成功后的一键拉起。
   - `setup_and_build`：作为各平台的总控制脚本。
3. **兼容性入口重定向**：
   - 覆写了根目录下的 `setup_build.sh`、`setup_build.cmd` 和 `setup_env.cmd`，自动根据当前平台路由到对应的 `setup/` 物理子脚本中，既保证了原有使用习惯，又彻底保洁了根目录。

---

## 物理文件清单

*   [docs/superpowers/plans/2026-06-14-multiplatform-env-setup.md](file:///Users/hanbozhang/C++/GRPD/docs/superpowers/plans/2026-06-14-multiplatform-env-setup.md) (设计计划书)
*   [setup/win/check_env.ps1](file:///Users/hanbozhang/C++/GRPD/setup/win/check_env.ps1)
*   [setup/win/install_deps.ps1](file:///Users/hanbozhang/C++/GRPD/setup/win/install_deps.ps1)
*   [setup/win/build_project.bat](file:///Users/hanbozhang/C++/GRPD/setup/win/build_project.bat)
*   [setup/win/setup_and_build.bat](file:///Users/hanbozhang/C++/GRPD/setup/win/setup_and_build.bat)
*   [setup/mac/check_env.sh](file:///Users/hanbozhang/C++/GRPD/setup/mac/check_env.sh)
*   [setup/mac/install_deps.sh](file:///Users/hanbozhang/C++/GRPD/setup/mac/install_deps.sh)
*   [setup/mac/build_project.sh](file:///Users/hanbozhang/C++/GRPD/setup/mac/build_project.sh)
*   [setup/mac/setup_and_build.sh](file:///Users/hanbozhang/C++/GRPD/setup/mac/setup_and_build.sh)
*   [setup/linux/check_env.sh](file:///Users/hanbozhang/C++/GRPD/setup/linux/check_env.sh)
*   [setup/linux/install_deps.sh](file:///Users/hanbozhang/C++/GRPD/setup/linux/install_deps.sh)
*   [setup/linux/build_project.sh](file:///Users/hanbozhang/C++/GRPD/setup/linux/build_project.sh)
*   [setup/linux/setup_and_build.sh](file:///Users/hanbozhang/C++/GRPD/setup/linux/setup_and_build.sh)
*   [setup_build.sh](file:///Users/hanbozhang/C++/GRPD/setup_build.sh) (路由更新)
*   [setup_build.cmd](file:///Users/hanbozhang/C++/GRPD/setup_build.cmd) (路由更新)
*   [setup_env.cmd](file:///Users/hanbozhang/C++/GRPD/setup_env.cmd) (路由更新)

---

## 验证与测试

我们可以在 macOS 平台上运行根目录下的一键构建入口：
```bash
./setup_build.sh
```
预期输出：
1. 输出 `GRPD One-Click Setup & Build for macOS`
2. 输出 `[1/3] Checking Environment...` 并执行 `check_env.sh`
3. 输出 `[2/3] Installing Dependencies & Virtual Env...` 并完成虚拟环境配置与 MCP 全局/本地绑定。
4. 输出 `[3/3] Compiling Project & Running...` 并编译启动 GRPD 引擎。
