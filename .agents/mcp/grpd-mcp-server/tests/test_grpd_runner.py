import subprocess
import os
import sys

import pytest

import grpd_runner


def test_run_grpd_case_allows_missing_grpd_when_yaml_exists(tmp_path, monkeypatch):
    case_dir = tmp_path / "ThermalCase"
    case_dir.mkdir()
    (case_dir / "PD.yaml").write_text("Parts: []\n", encoding="utf-8")

    calls = {}

    monkeypatch.setattr(
        grpd_runner,
        "resolve_grpd_executable",
        lambda grpd_root=None, executable="": "D:/fake/GRPD.exe",
    )

    def fake_run(args, **kwargs):
        calls["args"] = args
        calls["cwd"] = kwargs.get("cwd")
        return subprocess.CompletedProcess(args, 0, stdout="ok", stderr="")

    monkeypatch.setattr(grpd_runner.subprocess, "run", fake_run)

    result = grpd_runner.run_grpd_case(str(case_dir))

    assert result["success"] is True
    assert calls["args"] == ["D:/fake/GRPD.exe"]
    assert calls["cwd"] == str(case_dir)
    assert not (case_dir / "ThermalCase.grpd").exists()


def test_run_grpd_case_passes_real_python_environment_to_grpd(tmp_path, monkeypatch):
    case_dir = tmp_path / "PythonEnvCase"
    case_dir.mkdir()
    (case_dir / "PD.yaml").write_text("Parts: []\n", encoding="utf-8")

    monkeypatch.setenv(
        "PATH",
        os.pathsep.join(
            [
                r"C:\Program Files\WindowsApps",
                r"C:\OtherTool",
            ]
        ),
    )
    monkeypatch.setattr(
        grpd_runner,
        "resolve_grpd_executable",
        lambda grpd_root=None, executable="": "D:/fake/GRPD.exe",
    )

    calls = {}

    def fake_run(args, **kwargs):
        calls["env"] = kwargs.get("env")
        return subprocess.CompletedProcess(args, 0, stdout="ok", stderr="")

    monkeypatch.setattr(grpd_runner.subprocess, "run", fake_run)

    grpd_runner.run_grpd_case(str(case_dir))

    assert calls["env"] is not None
    assert calls["env"]["GRPD_PYTHON_EXE"] == sys.executable

    path_entries = calls["env"]["PATH"].split(os.pathsep)
    assert path_entries[0] == os.path.dirname(sys.executable)
    assert path_entries[1] == os.path.join(os.path.dirname(sys.executable), "Scripts")
    assert r"C:\Program Files\WindowsApps" in path_entries[2:]


def test_run_grpd_case_requires_pd_yaml_before_launching_grpd(tmp_path, monkeypatch):
    case_dir = tmp_path / "EmptyCase"
    case_dir.mkdir()

    monkeypatch.setattr(
        grpd_runner,
        "resolve_grpd_executable",
        lambda grpd_root=None, executable="": "D:/fake/GRPD.exe",
    )

    def fail_if_called(*args, **kwargs):
        raise AssertionError("GRPD should not run when PD.yaml is missing")

    monkeypatch.setattr(grpd_runner.subprocess, "run", fail_if_called)

    with pytest.raises(FileNotFoundError, match="PD.yaml not found"):
        grpd_runner.run_grpd_case(str(case_dir))
