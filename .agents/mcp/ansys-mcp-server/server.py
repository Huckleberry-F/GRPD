"""
server.py - ANSYS MCP Server 主程序
启动 MCP 服务器并注册供 AI 使用的 Tools 与 Resources。
"""

import os
import yaml
import sqlite3
from mcp.server.fastmcp import FastMCP
from apdl_generator import generate_apdl_from_yaml as _generate_apdl_from_yaml
from ansys_runner import AnsysRunner
from result_parser import ResultParser
from compare_and_export import compare_grpd_and_ansys

# SQLite 数据库文件路径
DB_FILE = os.path.join(os.path.dirname(__file__), "validation_history.db")

def save_ansys_comparison_to_db(db_path: str, vtk_file: str, ansys_txt_file: str, max_error_uy_percent: float, max_error_seqv_percent: float, output_dir: str, excel_path: str, plot_path: str, zip_path: str, **kwargs):
    """将 ANSYS 与 GRPD 的路径对比结果存入 SQLite 数据库"""
    conn = sqlite3.connect(db_path)
    cursor = conn.cursor()
    cursor.execute("""
        CREATE TABLE IF NOT EXISTS ansys_comparison_history (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
            vtk_file TEXT,
            ansys_txt_file TEXT,
            max_error_uy_percent REAL,
            max_error_seqv_percent REAL,
            output_dir TEXT,
            excel_path TEXT,
            plot_path TEXT,
            zip_path TEXT,
            max_error_temp_percent REAL
        )
    """)
    # Attempt to add the new column for backwards compatibility
    try:
        cursor.execute("ALTER TABLE ansys_comparison_history ADD COLUMN max_error_temp_percent REAL")
    except sqlite3.OperationalError:
        pass # Column might already exist

    cursor.execute("""
        INSERT INTO ansys_comparison_history
        (vtk_file, ansys_txt_file, max_error_uy_percent, max_error_seqv_percent, output_dir, excel_path, plot_path, zip_path, max_error_temp_percent)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)
    """, (
        vtk_file,
        ansys_txt_file,
        max_error_uy_percent,
        max_error_seqv_percent,
        output_dir,
        excel_path,
        plot_path,
        zip_path,
        kwargs.get('max_error_temp_percent', 0.0)
    ))
    conn.commit()
    conn.close()


def get_next_work_dir(base_dir: str, prefix: str = "run_") -> str:
    """在 base_dir 下寻找下一个递增编号的文件夹路径"""
    os.makedirs(base_dir, exist_ok=True)
    existing_folders = [f for f in os.listdir(base_dir) if os.path.isdir(os.path.join(base_dir, f))]

    max_idx = 0
    for folder in existing_folders:
        if folder.startswith(prefix):
            try:
                idx = int(folder[len(prefix):])
                if idx > max_idx:
                    max_idx = idx
            except ValueError:
                pass

    next_idx = max_idx + 1
    return os.path.join(base_dir, f"{prefix}{next_idx:04d}")

def save_ansys_validation_to_db(db_path: str, run_res: dict, yaml_file: str, substep: int, job_name: str):
    """将 ANSYS 校验结果存入 SQLite 数据库"""
    conn = sqlite3.connect(db_path)
    cursor = conn.cursor()
    cursor.execute("""
        CREATE TABLE IF NOT EXISTS ansys_smoke_test_validation (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
            yaml_file TEXT,
            substep INTEGER,
            job_name TEXT,
            elapsed_seconds REAL,
            passed INTEGER,
            work_dir TEXT,
            mac_file TEXT,
            ansys_txt_file TEXT,
            db_file TEXT,
            error_log_tail TEXT
        )
    """)

    passed = 1 if run_res.get("success", False) else 0
    cursor.execute("""
        INSERT INTO ansys_smoke_test_validation
        (yaml_file, substep, job_name, elapsed_seconds, passed, work_dir, mac_file, ansys_txt_file, db_file, error_log_tail)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    """, (
        yaml_file,
        substep,
        job_name,
        run_res.get("elapsed_seconds", 0.0),
        passed,
        run_res.get("work_dir", ""),
        run_res.get("mac_file", ""),
        run_res.get("ansys_txt_file", ""),
        run_res.get("db_file", ""),
        run_res.get("error_log_tail", "")
    ))
    conn.commit()
    conn.close()

# 加载配置
CONFIG_FILE = os.path.join(os.path.dirname(__file__), "config.yaml")
with open(CONFIG_FILE, "r", encoding="utf-8") as f:
    config = yaml.safe_load(f)

allowed_dirs = config.get("allowed_directories", [])
default_base_work_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), "work_dir"))
if default_base_work_dir not in allowed_dirs:
    allowed_dirs.append(default_base_work_dir)

runner = AnsysRunner(
    executable=config.get("ansys_executable", ""),
    num_processors=config.get("defaults", {}).get("num_processors", 4),
    memory_mb=config.get("defaults", {}).get("memory_mb", 2048),
    timeout_seconds=config.get("defaults", {}).get("timeout_seconds", 3600),
    allowed_directories=allowed_dirs,
)

mcp = FastMCP("AnsysServer")

@mcp.tool()
def run_ansys_mac(mac_file: str, work_dir: str = "", job_name: str = "", parameters: dict = None) -> dict:
    """
    运行 ANSYS MAPDL 批处理任务。支持动态向 APDL 注入参数。
    :param mac_file: .mac 脚本文件的绝对路径。
    :param work_dir: 工作目录绝对路径（留空则默认为 mac_file 同目录）。
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
    work_dir: str = "",
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
    if not work_dir:
        base_work_dir = os.path.join(os.path.dirname(__file__), "work_dir")
        work_dir = get_next_work_dir(base_work_dir)
    else:
        work_dir = os.path.abspath(work_dir)
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

    res_dict = {
        "success": success,
        "work_dir": work_dir,
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

    try:
        save_ansys_validation_to_db(DB_FILE, res_dict, yaml_file, substep, job_name)
    except Exception as db_err:
        res_dict["db_warning"] = f"写入 SQLite 数据库失败: {str(db_err)}"

    return res_dict

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
    :param work_dir: 工作目录 of the absolute path.
    """
    return ResultParser.list_result_files(work_dir)

@mcp.tool()
def generate_comparison_report(
    vtk_file: str,
    ansys_txt_file: str,
    output_dir: str = "",
    start_x: float = 0.5, start_y: float = 0.0, start_z: float = 0.0,
    end_x: float = 0.5, end_y: float = 5.0, end_z: float = 0.0,
    physics_type: str = "Mechanical",
    components: list = None,
    tol: float = None
) -> dict:
    """
    读取 GRPD 求解器输出的 VTK 文件与 ANSYS 的文本结果文件，自适应投影对齐并计算多物理场相对误差。
    支持任意三维斜线对角采样、自定义通道过滤与自适应容差。
    :param vtk_file: GRPD 输出的二进制 VTK 文件的绝对路径。
    :param ansys_txt_file: ANSYS 导出的 TXT 结果文件的绝对路径。
    :param output_dir: 生成报告的保存目录。
    :param start_x, start_y, start_z: 采样直线起始点坐标。
    :param end_x, end_y, end_z: 采样直线终止点坐标。
    :param physics_type: "Mechanical" 或 "Thermal"。
    :param components: 待对标的物理场分量列表，如 ["UX", "UY", "SEQV"]。
    :param tol: 粒子过滤几何垂直距离容差 (mm)，不传则自适应估算。
    """
    if not output_dir:
        base_work_dir = os.path.join(os.path.dirname(__file__), "work_dir")
        output_dir = get_next_work_dir(base_work_dir)
    else:
        output_dir = os.path.abspath(output_dir)

    os.makedirs(output_dir, exist_ok=True)

    line_start = (start_x, start_y, start_z)
    line_end = (end_x, end_y, end_z)

    result = compare_grpd_and_ansys(
        vtk_file=vtk_file,
        ansys_txt_file=ansys_txt_file,
        output_dir=output_dir,
        line_start=line_start,
        line_end=line_end,
        physics_type=physics_type,
        components=components,
        tol=tol
    )

    # 写入 SQLite 数据库
    try:
        save_ansys_comparison_to_db(
            db_path=DB_FILE,
            vtk_file=vtk_file,
            ansys_txt_file=ansys_txt_file,
            max_error_uy_percent=result.get("max_error_uy_percent", 0.0),
            max_error_seqv_percent=result.get("max_error_seqv_percent", 0.0),
            output_dir=output_dir,
            excel_path=result.get("excel_path", ""),
            plot_path=result.get("plot_path", ""),
            zip_path=result.get("zip_path", ""),
            max_error_temp_percent=result.get("max_error_temp_percent", 0.0)
        )
    except Exception as db_err:
        result["db_warning"] = f"写入 SQLite 数据库失败: {str(db_err)}"

    return result

if __name__ == "__main__":
    mcp.run()
