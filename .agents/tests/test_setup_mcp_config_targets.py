import importlib.util
import json
import sys
import types
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]


def _load_setup_mcp():
    setup_path = ROOT / ".agents" / "setup_mcp.py"
    spec = importlib.util.spec_from_file_location("setup_mcp_under_test", setup_path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def test_setup_mcp_writes_all_antigravity_config_roots(tmp_path, monkeypatch):
    setup_mcp = _load_setup_mcp()

    fake_agents_dir = tmp_path / "project" / ".agents"
    fake_agents_dir.mkdir(parents=True)
    fake_project_root = fake_agents_dir.parent
    fake_home = tmp_path / "home"

    example_config = {
        "mcpServers": {
            "new-service": {
                "command": "python",
                "args": [
                    "<GRPD_PROJECT_ROOT_PATH>\\.agents\\mcp\\new-service\\server.py",
                ],
                "cwd": "<GRPD_PROJECT_ROOT_PATH>\\.agents\\mcp\\new-service",
                "timeout": 12345,
            }
        }
    }
    (fake_agents_dir / "settings.json.example").write_text(
        json.dumps(example_config),
        encoding="utf-8",
    )

    for name in ("config", "antigravity", "antigravity-ide", "antigravity-backup"):
        (fake_home / ".gemini" / name).mkdir(parents=True)

    monkeypatch.setattr(setup_mcp, "__file__", str(fake_agents_dir / "setup_mcp.py"))
    monkeypatch.setattr(setup_mcp.sys, "executable", "C:\\Python312\\python.exe")
    monkeypatch.setenv("USERPROFILE", str(fake_home))
    monkeypatch.setenv("HOME", str(fake_home))
    monkeypatch.setitem(sys.modules, "mcp", types.ModuleType("mcp"))

    setup_mcp.main()

    expected_script = str(
        fake_project_root / ".agents" / "mcp" / "new-service" / "server.py"
    )
    expected_cwd = str(fake_project_root / ".agents" / "mcp" / "new-service")

    for name in ("config", "antigravity", "antigravity-ide", "antigravity-backup"):
        config_path = fake_home / ".gemini" / name / "mcp_config.json"
        config = json.loads(config_path.read_text(encoding="utf-8"))
        service = config["mcpServers"]["new-service"]

        assert service["command"] == "C:\\Python312\\python.exe"
        assert service["args"] == [expected_script]
        assert service["cwd"] == expected_cwd
        assert service["timeout"] == 12345
