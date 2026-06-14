import importlib.util
import os
from pathlib import Path
import zipfile

import numpy as np
import pyvista as pv


SERVER_FILE = Path(__file__).resolve().parents[1] / "server.py"
SPEC = importlib.util.spec_from_file_location("grpd_validation_server_under_test", SERVER_FILE)
SERVER = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(SERVER)
compare_grpd_vtk_with_ansys_txt = SERVER.compare_grpd_vtk_with_ansys_txt


def test_compare_grpd_vtk_with_ansys_txt_writes_artifacts_to_tmp_path(tmp_path):
    n_points = 21
    distance = np.linspace(0.0, 10.0, n_points)
    points = np.column_stack((distance, np.zeros(n_points), np.zeros(n_points)))

    mesh = pv.PolyData(points)
    mesh.point_data["Displacement"] = np.column_stack(
        (distance * 0.1, distance * 0.2, distance * 0.3)
    )
    mesh.point_data["VonMisesStress"] = distance * 10.0

    vtk_file = tmp_path / "mock_grpd.vtk"
    mesh.save(vtk_file)

    ansys_txt_file = tmp_path / "mock_ansys.txt"
    with ansys_txt_file.open("w", encoding="utf-8") as file:
        file.write("DIST UY SEQV\n")
        for value in distance:
            file.write(f"{value:.6f} {value * 0.2:.6f} {value * 10.0:.6f}\n")

    output_dir = tmp_path / "report"
    result = compare_grpd_vtk_with_ansys_txt(
        vtk_file=str(vtk_file),
        ansys_txt_file=str(ansys_txt_file),
        output_dir=str(output_dir),
        start_x=0.0,
        start_y=0.0,
        start_z=0.0,
        end_x=10.0,
        end_y=0.0,
        end_z=0.0,
        physics_type="Mechanical",
        components=["UY", "SEQV"],
        tol=0.01,
    )

    assert result["success"] is True
    assert result["max_error_uy_percent"] < 1.0e-9
    assert result["max_error_seqv_percent"] < 1.0e-9

    for key in ("excel_path", "plot_path", "summary_path", "zip_path"):
        assert os.path.exists(result[key])
        assert os.path.commonpath([str(tmp_path), result[key]]) == str(tmp_path)

    with zipfile.ZipFile(result["zip_path"], "r") as archive:
        assert set(archive.namelist()) == {
            "Comparison_Report.xlsx",
            "Comparison_Plot.png",
            "Comparison_Summary.json",
        }
