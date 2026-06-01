from pathlib import Path


SERVER_ROOT = Path(__file__).resolve().parents[1]


def test_validation_mcp_has_standard_supporting_directories():
    assert (SERVER_ROOT / "src" / "__init__.py").is_file()
    assert (SERVER_ROOT / "src" / "service.py").is_file()
    assert (SERVER_ROOT / "tests" / "__init__.py").is_file()
    assert (SERVER_ROOT / "templates" / "README.md").is_file()


def test_validation_mcp_does_not_require_runner_or_generator_modules():
    assert not (SERVER_ROOT / "src" / "runner.py").exists()
    assert not (SERVER_ROOT / "src" / "generator.py").exists()


def test_validation_server_routes_through_service_layer():
    content = (SERVER_ROOT / "server.py").read_text(encoding="utf-8")

    assert "from src.service import" in content
    assert "from src.comparison import" not in content
    assert "from src.paths import" not in content
