"""
ansys_runner.py - ANSYS MAPDL 批处理执行引擎

负责：
1. 调用本地 ANSYS MAPDL 可执行文件进行批处理计算
2. 管理工作目录和临时文件
3. 捕获求解日志和错误信息
4. 解析 ANSYS 输出文件中的关键数值结果
"""

import subprocess
import os
import time
import re
import glob
from pathlib import Path
from dataclasses import dataclass, field
from typing import Optional


@dataclass
class SolveResult:
    """ANSYS 求解结果的数据容器"""

    success: bool
    job_name: str
    work_dir: str
    elapsed_seconds: float
    output_log: str  # .out 文件内容 (截断)
    error_log: str  # .err 文件内容 (截断)
    result_files: list[str] = field(default_factory=list)
    extracted_data: dict = field(default_factory=dict)
    message: str = ""


class AnsysRunner:
    """ANSYS MAPDL 批处理执行器"""

    # 每个日志文件最多读取的字符数，防止内存爆炸
    MAX_LOG_CHARS = 50000

    def __init__(
        self,
        executable: str,
        num_processors: int = 4,
        memory_mb: int = 2048,
        timeout_seconds: int = 3600,
        allowed_directories: Optional[list[str]] = None,
    ):
        self.executable = executable
        self.num_processors = num_processors
        self.memory_mb = memory_mb
        self.timeout_seconds = timeout_seconds
        self.allowed_directories = allowed_directories or []

    def _validate_path(self, path: str) -> bool:
        """检查路径是否在安全白名单内"""
        abs_path = os.path.normcase(os.path.abspath(path))
        if not self.allowed_directories:
            return True
        return any(
            abs_path.startswith(os.path.normcase(os.path.abspath(d)))
            for d in self.allowed_directories
        )

    def _read_file_tail(self, filepath: str, max_chars: int = 0) -> str:
        """安全读取文件末尾内容"""
        if max_chars <= 0:
            max_chars = self.MAX_LOG_CHARS
        try:
            if not os.path.isfile(filepath):
                return ""
            size = os.path.getsize(filepath)
            with open(filepath, "r", encoding="utf-8", errors="replace") as f:
                if size > max_chars:
                    f.seek(size - max_chars)
                    # 跳过不完整行
                    f.readline()
                return f.read(max_chars)
        except Exception as e:
            return f"[读取文件失败: {e}]"

    def _collect_result_files(self, work_dir: str, job_name: str) -> list[str]:
        """收集求解产生的结果文件列表"""
        result_extensions = [
            ".rst",   # 结构结果
            ".rth",   # 热分析结果
            ".rnnn",  # 非线性结果
            ".out",   # 输出日志
            ".err",   # 错误日志
            ".txt",   # 文本导出
            ".csv",   # CSV 导出
            ".db",    # 数据库
        ]
        found = []
        for ext in result_extensions:
            pattern = os.path.join(work_dir, f"{job_name}*{ext}")
            found.extend(glob.glob(pattern))
        # 也收集所有 .txt 文件
        for txt in glob.glob(os.path.join(work_dir, "*.txt")):
            if txt not in found:
                found.append(txt)
        return sorted(found)

    def _extract_result_data(
        self, work_dir: str, job_name: str
    ) -> dict:
        """从 ANSYS 输出文件中提取关键数值数据"""
        data: dict = {}

        # 1. 尝试读取文本导出文件 (如 ansys_results.txt)
        for txt_file in glob.glob(os.path.join(work_dir, "*.txt")):
            basename = os.path.basename(txt_file)
            content = self._read_file_tail(txt_file, 20000)
            if content.strip():
                data[basename] = self._parse_ansys_table(content)

        # 2. 从 .out 文件提取收敛信息
        out_files = glob.glob(os.path.join(work_dir, f"{job_name}*.out"))
        for out_file in out_files:
            content = self._read_file_tail(out_file)
            convergence = self._parse_convergence_info(content)
            if convergence:
                data["convergence"] = convergence

        return data

    def _parse_ansys_table(self, content: str) -> dict:
        """解析 ANSYS PRVAR/PRNSOL 风格的表格输出"""
        lines = content.strip().splitlines()
        result: dict = {"raw": content[:5000]}

        # 尝试检测数值行（以数字开头的行）
        numeric_rows = []
        for line in lines:
            stripped = line.strip()
            if not stripped:
                continue
            # ANSYS 表格通常是空格分隔的数值
            parts = stripped.split()
            try:
                values = [float(p) for p in parts]
                numeric_rows.append(values)
            except ValueError:
                continue

        if numeric_rows:
            result["num_rows"] = len(numeric_rows)
            result["num_cols"] = len(numeric_rows[0]) if numeric_rows else 0
            # 提取每列的最大最小值
            if numeric_rows:
                n_cols = len(numeric_rows[0])
                for col_idx in range(n_cols):
                    col_vals = [
                        row[col_idx]
                        for row in numeric_rows
                        if col_idx < len(row)
                    ]
                    if col_vals:
                        result[f"col{col_idx}_min"] = min(col_vals)
                        result[f"col{col_idx}_max"] = max(col_vals)
                # 返回前 5 行和后 5 行作为预览
                preview = numeric_rows[:5]
                if len(numeric_rows) > 10:
                    preview.extend(numeric_rows[-5:])
                elif len(numeric_rows) > 5:
                    preview.extend(numeric_rows[5:])
                result["preview_rows"] = preview

        return result

    def _parse_convergence_info(self, content: str) -> dict:
        """从 .out 文件中提取收敛状态信息"""
        info: dict = {}

        # 查找子步完成标记
        substep_pattern = re.compile(
            r"SUBSTEP\s+(\d+)\s+COMPLETED", re.IGNORECASE
        )
        substeps = substep_pattern.findall(content)
        if substeps:
            info["completed_substeps"] = len(substeps)
            info["last_substep"] = int(substeps[-1])

        # 查找总求解时间
        time_pattern = re.compile(
            r"ELAPSED\s+CP\s+TIME\s*[\(SEC\)]*\s*=\s*([\d.]+)", re.IGNORECASE
        )
        times = time_pattern.findall(content)
        if times:
            info["ansys_cp_time_sec"] = float(times[-1])

        # 查找迭代次数
        equil_pattern = re.compile(
            r"EQUIL\s+ITER\s+(\d+)\s+COMPLETED", re.IGNORECASE
        )
        equils = equil_pattern.findall(content)
        if equils:
            info["total_equilibrium_iterations"] = len(equils)

        # 检查是否有致命错误
        error_pattern = re.compile(r"\*\*\* ERROR \*\*\*", re.IGNORECASE)
        errors = error_pattern.findall(content)
        if errors:
            info["fatal_errors"] = len(errors)

        # 检查警告
        warn_pattern = re.compile(r"\*\*\* WARNING \*\*\*", re.IGNORECASE)
        warns = warn_pattern.findall(content)
        if warns:
            info["warnings"] = len(warns)

        return info

    def run_mac_file(
        self,
        mac_file_path: str,
        work_dir: Optional[str] = None,
        job_name: Optional[str] = None,
        extra_args: Optional[list[str]] = None,
        parameters: Optional[dict] = None,
    ) -> SolveResult:
        """
        执行一个 ANSYS APDL .mac 文件

        参数:
            mac_file_path: .mac 文件的绝对路径
            work_dir: 工作目录 (默认为 .mac 文件所在目录)
            job_name: ANSYS 作业名 (默认从文件名推断)
            extra_args: 额外的命令行参数

        返回:
            SolveResult 对象
        """
        mac_file_path = os.path.abspath(mac_file_path)

        if not os.path.isfile(mac_file_path):
            return SolveResult(
                success=False,
                job_name="",
                work_dir="",
                elapsed_seconds=0.0,
                output_log="",
                error_log="",
                message=f"MAC 文件不存在: {mac_file_path}",
            )

        if work_dir is None:
            work_dir = os.path.dirname(mac_file_path)
        work_dir = os.path.abspath(work_dir)

        if not self._validate_path(work_dir):
            return SolveResult(
                success=False,
                job_name="",
                work_dir=work_dir,
                elapsed_seconds=0.0,
                output_log="",
                error_log="",
                message=f"工作目录不在安全白名单内: {work_dir}",
            )

        if job_name is None:
            job_name = Path(mac_file_path).stem

        if parameters:
            wrapper_path = os.path.join(work_dir, f"mcp_wrapper_{job_name}.mac")
            with open(wrapper_path, "w", encoding="utf-8") as fw:
                fw.write("! === MCP INJECTED PARAMETERS ===\n")
                for k, v in parameters.items():
                    fw.write(f"{k} = {v}\n")
                fw.write("! === END MCP PARAMETERS ===\n\n")
                with open(mac_file_path, "r", encoding="utf-8", errors="replace") as fr:
                    fw.write(fr.read())
            actual_mac_file = wrapper_path
        else:
            actual_mac_file = mac_file_path

        # 构建 ANSYS 命令行
        cmd = [
            self.executable,
            "-b",                          # 批处理模式
            "-i", actual_mac_file,         # 输入文件
            "-o", os.path.join(work_dir, f"{job_name}.out"),  # 输出文件
            "-j", job_name,                # 作业名
            "-dir", work_dir,              # 工作目录
            "-np", str(self.num_processors),  # CPU 核心数
            "-m", str(self.memory_mb),     # 内存 (MB)
        ]

        if extra_args:
            cmd.extend(extra_args)

        # 设置环境变量：防止弹出对话框，并注入 PATH 防止 DLL 找不到
        env = os.environ.copy()
        ansys_bin_dir = os.path.dirname(self.executable)
        if ansys_bin_dir and ansys_bin_dir not in env.get("PATH", ""):
            env["PATH"] = ansys_bin_dir + os.pathsep + env.get("PATH", "")
        env["ANS_CONSEC"] = "YES"

        start_time = time.time()

        try:
            process = subprocess.run(
                cmd,
                cwd=work_dir,
                env=env,
                capture_output=True,
                text=True,
                timeout=self.timeout_seconds,
            )
            elapsed = time.time() - start_time
            returncode = process.returncode
        except subprocess.TimeoutExpired:
            elapsed = time.time() - start_time
            return SolveResult(
                success=False,
                job_name=job_name,
                work_dir=work_dir,
                elapsed_seconds=elapsed,
                output_log="",
                error_log="",
                message=f"ANSYS 求解超时 ({self.timeout_seconds}s)",
            )
        except FileNotFoundError:
            return SolveResult(
                success=False,
                job_name=job_name,
                work_dir=work_dir,
                elapsed_seconds=0.0,
                output_log="",
                error_log="",
                message=f"找不到 ANSYS 可执行文件: {self.executable}",
            )
        except Exception as e:
            elapsed = time.time() - start_time
            return SolveResult(
                success=False,
                job_name=job_name,
                work_dir=work_dir,
                elapsed_seconds=elapsed,
                output_log="",
                error_log="",
                message=f"执行异常: {e}",
            )

        # 读取输出日志
        out_file = os.path.join(work_dir, f"{job_name}.out")
        err_file = os.path.join(work_dir, f"{job_name}.err")
        output_log = self._read_file_tail(out_file)
        error_log = self._read_file_tail(err_file)

        # 收集结果文件
        result_files = self._collect_result_files(work_dir, job_name)

        # 提取数值数据
        extracted_data = self._extract_result_data(work_dir, job_name)

        success = returncode == 0
        message = "求解完成" if success else f"ANSYS 返回错误码: {returncode}"

        # 检查 .err 文件中的致命错误
        if "*** ERROR ***" in error_log:
            success = False
            message = "ANSYS 报告了致命错误，请检查 error_log"

        return SolveResult(
            success=success,
            job_name=job_name,
            work_dir=work_dir,
            elapsed_seconds=elapsed,
            output_log=output_log,
            error_log=error_log,
            result_files=result_files,
            extracted_data=extracted_data,
            message=message,
        )
