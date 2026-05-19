"""
server.py - ANSYS MCP Server 主程序

启动 MCP 服务器并注册供 AI 使用的 Tools 和 Resources。
"""

import os
import yaml
from mcp.server.fastmcp import FastMCP
from ansys_runner import AnsysRunner
from result_parser import ResultParser

# 加载配置
CONFIG_FILE = os.path.join(os.path.dirname(__file__), "config.yaml")
with open(CONFIG_FILE, "r", encoding="utf-8") as f:
    config = yaml.safe_load(f)

runner = AnsysRunner(
    executable=config.get("ansys_executable", ""),
    num_processors=config.get("defaults", {}).get("num_processors", 4),
    memory_mb=config.get("defaults", {}).get("memory_mb", 2048),
    timeout_seconds=config.get("defaults", {}).get("timeout_seconds", 3600),
    allowed_directories=config.get("allowed_directories", []),
)

mcp = FastMCP("AnsysServer")

@mcp.tool()
def run_ansys_mac(mac_file: str, work_dir: str = "", job_name: str = "", parameters: dict = None) -> dict:
    """
    运行 ANSYS MAPDL 批处理任务。支持动态向 APDL 注入参数。
    :param mac_file: .mac 脚本文件的绝对路径。
    :param work_dir: 工作目录绝对路径（留空则默认与 mac_file 同目录）。
    :param job_name: 任务名称（留空则默认使用 mac_file 的文件名）。
    :param parameters: 字典形式的参数集，会在执行前以 Wrapper Macro 形式注入 APDL（如 {"START_X": 0.5, "START_Y": 0.0}）。
    """
    if not work_dir:
        work_dir = os.path.dirname(mac_file)
    result = runner.run_mac_file(mac_file, work_dir, job_name, parameters=parameters)
    return {
        "success": result.success,
        "job_name": result.job_name,
        "elapsed_seconds": result.elapsed_seconds,
        "message": result.message,
        "error_log_tail": result.error_log[-2000:] if result.error_log else "",
        "result_files_count": len(result.result_files)
    }

@mcp.tool()
def get_ansys_solve_status(out_file: str) -> dict:
    """
    解析 ANSYS .out 日志文件，获取求解收敛状态、警告和错误信息。
    :param out_file: .out 文件的绝对路径。
    """
    return ResultParser.parse_out_file(out_file)

@mcp.tool()
def get_ansys_text_results(txt_file: str) -> dict:
    """
    解析 ANSYS PRVAR/PRNSOL 导出的纯文本数据文件 (.txt)。
    返回列名、数据行数、极值以及预览数据，供进一步数值分析。
    :param txt_file: .txt 结果文件的绝对路径。
    """
    return ResultParser.parse_prvar_output(txt_file)

@mcp.tool()
def list_ansys_files(work_dir: str) -> dict:
    """
    列出指定工作目录中的所有 ANSYS 结果文件和日志文件。
    :param work_dir: 工作目录的绝对路径。
    """
    return ResultParser.list_result_files(work_dir)

if __name__ == "__main__":
    mcp.run()
