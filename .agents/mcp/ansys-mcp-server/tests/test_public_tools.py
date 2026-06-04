import importlib.util
from pathlib import Path


SERVER_FILE = Path(__file__).resolve().parents[1] / "server.py"
SPEC = importlib.util.spec_from_file_location("ansys_server_under_test", SERVER_FILE)
assert SPEC is not None
server = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(server)


def test_ansys_server_keeps_only_ansys_side_tools():
    assert hasattr(server, "run_ansys_mac")
    assert hasattr(server, "generate_ansys_apdl_from_yaml")
    assert hasattr(server, "run_ansys_yaml_case")
    assert hasattr(server, "get_ansys_solve_status")
    assert hasattr(server, "get_ansys_text_results")
    assert hasattr(server, "list_ansys_files")
    assert not hasattr(server, "generate_comparison_report")
