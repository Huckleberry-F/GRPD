from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]


def test_smoke_tester_docs_use_validation_mcp_and_no_legacy_script():
    paths = [
        ROOT / ".agents" / "skills" / "superCAE" / "skills" / "grpd-smoke-tester" / "SKILL.md",
        ROOT
        / ".agents"
        / "skills"
        / "superCAE"
        / "skills"
        / "grpd-smoke-tester"
        / "references"
        / "smoke-test-playbook.md",
    ]
    content = "\n".join(path.read_text(encoding="utf-8") for path in paths)

    assert "grpd-validation-server.compare_grpd_vtk_with_ansys_txt" in content
    assert "ansys-server.generate_comparison_report" not in content
    assert "run_thermal_smoke_test.py" not in content
    assert not (ROOT / ".agents" / "run_thermal_smoke_test.py").exists()
