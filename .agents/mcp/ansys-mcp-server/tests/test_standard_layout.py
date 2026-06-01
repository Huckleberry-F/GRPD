from pathlib import Path


SERVER_ROOT = Path(__file__).resolve().parents[1]


def test_ansys_mcp_uses_standard_src_layout():
    expected_src_files = {
        "__init__.py",
        "paths.py",
        "runner.py",
        "generator.py",
        "result_parser.py",
        "history.py",
        "service.py",
    }
    src_dir = SERVER_ROOT / "src"

    assert src_dir.is_dir()
    assert expected_src_files.issubset({path.name for path in src_dir.iterdir()})


def test_ansys_mcp_has_no_root_core_compatibility_modules():
    legacy_modules = {
        "ansys_runner.py",
        "apdl_generator.py",
        "result_parser.py",
    }

    for module_name in legacy_modules:
        assert not (SERVER_ROOT / module_name).exists()


def test_ansys_mcp_has_templates_directory():
    assert (SERVER_ROOT / "templates" / "README.md").is_file()


def test_ansys_src_modules_do_not_keep_direct_cli_entries():
    for module_name in ("runner.py", "generator.py"):
        content = (SERVER_ROOT / "src" / module_name).read_text(encoding="utf-8")
        assert 'if __name__ == "__main__"' not in content
