"""FastMCP facade for GRPD validation post-processing tools."""

from __future__ import annotations

import os
import sys

from mcp.server.fastmcp import FastMCP


SERVER_DIR = os.path.dirname(os.path.abspath(__file__))


def _activate_local_src_package() -> None:
    """Prefer this service's local ``src`` package during facade imports."""
    local_src = os.path.join(SERVER_DIR, "src")
    existing = sys.modules.get("src")
    existing_file = getattr(existing, "__file__", "") if existing else ""
    if existing and not os.path.abspath(existing_file).startswith(local_src):
        for name in list(sys.modules):
            if name == "src" or name.startswith("src."):
                del sys.modules[name]

    if SERVER_DIR in sys.path:
        sys.path.remove(SERVER_DIR)
    sys.path.insert(0, SERVER_DIR)


_activate_local_src_package()

from src.service import (
    compare_grpd_vtk_with_ansys_txt as _compare_grpd_vtk_with_ansys_txt,
)


mcp = FastMCP("GRPDValidationServer")


@mcp.tool()
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
    """
    璇诲彇 GRPD VTK 涓?ANSYS TXT锛屾部涓夌淮閲囨牱绾垮榻愬苟鐢熸垚璇樊鎶ュ憡銆?    鏈伐鍏锋槸 GRPD/ANSYS 瀵规爣鎶ュ憡鍞竴鍏ュ彛锛汚NSYS MCP 涓嶅啀鍏紑瀵规瘮鎶ュ憡宸ュ叿銆?    """
    return _compare_grpd_vtk_with_ansys_txt(
        vtk_file,
        ansys_txt_file,
        output_dir,
        start_x,
        start_y,
        start_z,
        end_x,
        end_y,
        end_z,
        physics_type,
        components,
        tol,
    )


if __name__ == "__main__":
    mcp.run()
