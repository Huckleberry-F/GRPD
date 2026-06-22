# MATLAB MCP 服务跨平台可执行文件命名修复 Walkthrough

## 问题背景

在 macOS 环境下调用 `matlab-server` 的 `run_constitutive_validation` 校验本构时，出现以下异常报错：
```
校验管道发生未捕获异常: 在编译成功后未找到 test_constitutive.exe 可执行文件！
```
这是因为在 `.agents/mcp/matlab-mcp-server/server.py` 的可执行程序查找逻辑中，硬编码了 `test_constitutive.exe` 作为搜寻名称。在 macOS/Linux 等类 Unix 系统下，编译生成的二进制文件实际为 `test_constitutive`（无后缀）。

## 修改内容

### [MODIFY] [server.py](file:///Users/hanbozhang/C++/GRPD/.agents/mcp/matlab-mcp-server/server.py)

在 `compile_and_locate_cpp_test` 函数中，通过 `os.name` 动态判定操作系统类型：
- 对于 Windows (`nt`) 系统，后缀设为 `.exe`；
- 对于类 Unix（macOS/Linux）系统，后缀设为空字符串 `""`。

```python
    suffix = ".exe" if os.name == "nt" else ""
    possible_paths = [
        os.path.join(root_dir, "bin", "Release", f"test_constitutive{suffix}"),
        os.path.join(root_dir, "bin", "release", f"test_constitutive{suffix}"),
        os.path.join(root_dir, "bin", f"test_constitutive{suffix}"),
        os.path.join(root_dir, "build", "bin", "Release", f"test_constitutive{suffix}"),
        os.path.join(root_dir, "build", "bin", f"test_constitutive{suffix}"),
    ]
```

## 验证与恢复机制

1. **临时恢复**：在常驻 MCP 服务进程重新加载此修改前，为了保证对标验证管道可用，我们在 `/Users/hanbozhang/C++/GRPD/bin/release` 目录下创建了 `test_constitutive.exe` 到 `test_constitutive` 的软链接。
2. **彻底解决**：用户后续重启 IDE 或重载 MCP 服务后，新的 `server.py` 将会自动依据系统类型查找 `test_constitutive`，实现原生的 macOS/Linux 支持。
