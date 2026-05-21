"""
server.py - ANSYS MCP Server 主程序

启动 MCP 服务器并注册供 AI 使用的 Tools 和 Resources。
"""

import os
import yaml
from mcp.server.fastmcp import FastMCP
from apdl_generator import generate_apdl_from_yaml as _generate_apdl_from_yaml
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
        "work_dir": result.work_dir,
        "elapsed_seconds": result.elapsed_seconds,
        "message": result.message,
        "error_log_tail": result.error_log[-2000:] if result.error_log else "",
        "result_files_count": len(result.result_files),
        "result_files": result.result_files,
    }

@mcp.tool()
def generate_ansys_apdl_from_yaml(
    yaml_file: str,
    output_mac: str,
    substep: int = 0,
    job_name: str = "ansys_smoke_test",
    start_x: float = 0.0,
    end_x: float = 0.0,
    start_y: float = 0.0,
    end_y: float = 0.0,
) -> dict:
    """
    根据 GRPD 的 PD.yaml 生成 ANSYS APDL 验证宏，并包含 SAVE 命令生成 .db 文件。
    坐标参数为 APDL 未收到动态参数时使用的默认采样路径。
    """
    result = _generate_apdl_from_yaml(
        yaml_file,
        output_mac,
        job_name=job_name,
        result_substep=substep,
        default_start_x=start_x,
        default_end_x=end_x if end_x != 0.0 else start_x,
        default_start_y=start_y,
        default_end_y=end_y if end_y != 0.0 else None,
    )
    result["ansys_txt_file"] = os.path.join(
        os.path.dirname(os.path.abspath(output_mac)),
        f"ansys_val_results_sub{substep}.txt" if substep else "ansys_val_results.txt",
    )
    result["substep"] = substep
    return result

@mcp.tool()
def run_ansys_yaml_case(
    yaml_file: str,
    work_dir: str,
    substep: int = 0,
    start_x: float = 1.0,
    end_x: float = 1.0,
    start_y: float = 0.0,
    end_y: float = 0.0,
    job_name: str = "ansys_smoke_test",
) -> dict:
    """
    由 GRPD PD.yaml 生成 APDL，运行 ANSYS，并返回 txt/db/out/err 等结果路径。
    """
    os.makedirs(work_dir, exist_ok=True)
    mac_file = os.path.join(work_dir, f"{job_name}.mac")
    _generate_apdl_from_yaml(
        yaml_file,
        mac_file,
        job_name=job_name,
        result_substep=substep,
        default_start_x=start_x,
        default_end_x=end_x,
        default_start_y=start_y,
        default_end_y=end_y if end_y != 0.0 else None,
    )

    parameters = {"START_X": start_x, "END_X": end_x}
    if start_y != 0.0:
        parameters["START_Y"] = start_y
    if end_y != 0.0:
        parameters["END_Y"] = end_y
    if substep:
        parameters["SUBSTEP"] = substep

    result = runner.run_mac_file(mac_file, work_dir, job_name, parameters=parameters)
    ansys_txt_file = os.path.join(
        work_dir,
        f"ansys_val_results_sub{substep}.txt" if substep else "ansys_val_results.txt",
    )
    db_file = os.path.join(work_dir, f"{job_name}.db")
    out_file = os.path.join(work_dir, f"{job_name}.out")
    err_file = os.path.join(work_dir, f"{job_name}.err")

    text_result = ResultParser.parse_prvar_output(ansys_txt_file)
    txt_ok = bool(text_result.get("success"))
    db_exists = os.path.exists(db_file)
    has_fatal_error = "*** ERROR ***" in (result.error_log or "")
    success = not has_fatal_error and txt_ok and db_exists

    warnings = []
    if not result.success:
        warnings.append(result.message)
    if not db_exists:
        warnings.append(f"ANSYS database file was not found: {db_file}")
    if not txt_ok:
        warnings.append(text_result.get("message", f"ANSYS text result was not parseable: {ansys_txt_file}"))

    return {
        "success": success,
        "mac_file": mac_file,
        "ansys_txt_file": ansys_txt_file,
        "db_file": db_file,
        "db_exists": db_exists,
        "out_file": out_file,
        "err_file": err_file,
        "result_files": result.result_files,
        "elapsed_seconds": result.elapsed_seconds,
        "message": result.message,
        "warnings": warnings,
        "error_log_tail": result.error_log[-2000:] if result.error_log else "",
        "text_result": text_result,
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
