# 修复 MCP 调用 GRPD.exe 时 Python 子进程管道死锁

## 问题根因

`grpd_runner.py` 使用 `subprocess.run(capture_output=True)` 启动 `GRPD.exe`，
stdout/stderr 被绑定到 **管道 (Pipe)**。`GRPD.exe` 内部再通过 `std::system()` 调用
Python 生成网格，Python 子进程 **继承了管道句柄**，导致 I/O 死锁。

直接在终端双击运行 GRPD.exe 时 stdout 连的是控制台，不存在管道继承，所以一直正常。

## 设计决策

| 决策点 | 结论 |
|--------|------|
| GRPD.exe 保留调 Python 的能力 | ✅ 保留 |
| Python 解释器定位方式 | 永远只用裸命令 `python`（从 PATH 发现），删除 `GRPD_PYTHON_EXE` 环境变量注入 |
| 死锁修复位置 | `grpd_runner.py` 改用临时文件重定向代替管道 |
| C++ 辅助函数 | 全部删除，还原最简调用逻辑 |

## 改动清单

### 1. `InitModel.cpp` — 删除所有辅助函数，简化 Python 调用

**删除**匿名命名空间中的 5 个函数：
- `stripOuterQuotes()`
- `quoteCommandArgument()`
- `isBareCommandName()`
- `formatCommandExecutable()`
- `resolvePythonExecutable()`

**简化**调用逻辑为：
```cpp
std::string pythonCommand =
    "python \"" + scriptPath.string() + "\" \"" + yamlPath + "\"";
int pyStatus = std::system(pythonCommand.c_str());
```

> **为什么不需要外层引号包裹？**
> Windows `cmd.exe` 的引号剥离规则：仅当命令字符串的**第一个字符**是 `"` 时才触发。
> 由于 `python` 是裸命令（以 `p` 开头），`cmd.exe` 不会剥离任何引号，
> 后面的路径参数用简单双引号即可安全处理空格。

### 2. `grpd_runner.py` — 消除管道，删除环境变量注入

- 删除 `_build_grpd_subprocess_env()` 函数
- `run_grpd_case()` 中的 `subprocess.run` 改为：
  - stdout/stderr 重定向到临时文件
  - `env` 参数移除（继承当前环境）
  - 运行结束后读回临时文件内容，清理文件

## 验证方式

1. 通过 MCP `grpd-server.get_grpd_vtk_result` 运行热传导算例，确认不再死锁
2. 直接终端运行 `GRPD.exe` 确认行为不变
