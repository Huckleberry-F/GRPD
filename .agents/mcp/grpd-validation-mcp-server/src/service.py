"""Service-level operations behind the GRPD validation MCP facade."""

from __future__ import annotations

import os

from .comparison import compare_grpd_and_ansys
from .paths import get_next_work_dir


def compare_grpd_vtk_with_ansys_txt(
    vtk_file: str,
    ansys_txt_file: str,
    output_dir: str = "",
    start_x: float = 0.5,
    start_y: float = 0.0,
    start_z: float = 0.0,
    end_x: float = 0.5,
    end_y: float = 5.0,
    end_z: float = 0.0,
    physics_type: str = "Mechanical",
    components: list | None = None,
    tol: float | None = None,
) -> dict:
    """Create a GRPD/ANSYS comparison report from VTK and TXT result files."""
    report_dir = os.path.abspath(output_dir) if output_dir else get_next_work_dir()
    os.makedirs(report_dir, exist_ok=True)

    return compare_grpd_and_ansys(
        vtk_file=vtk_file,
        ansys_txt_file=ansys_txt_file,
        output_dir=report_dir,
        line_start=(start_x, start_y, start_z),
        line_end=(end_x, end_y, end_z),
        physics_type=physics_type,
        components=components,
        tol=tol,
    )
