import importlib.util
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]


def _load_server(label: str, relative_path: str):
    server_path = ROOT / relative_path
    spec = importlib.util.spec_from_file_location(label, server_path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def test_ansys_and_validation_servers_can_be_imported_in_one_process():
    ansys = _load_server(
        "ansys_server_import_isolation_probe",
        ".agents/mcp/ansys-mcp-server/server.py",
    )
    validation = _load_server(
        "validation_server_import_isolation_probe",
        ".agents/mcp/grpd-validation-mcp-server/server.py",
    )

    assert hasattr(ansys, "run_ansys_yaml_case")
    assert not hasattr(ansys, "generate_comparison_report")
    assert hasattr(validation, "compare_grpd_vtk_with_ansys_txt")
    assert not hasattr(validation, "run_ansys_yaml_case")
