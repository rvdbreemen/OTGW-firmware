"""End-to-end tests for bin/adr-lint.

Each test runs the CLI as a subprocess and asserts on the JSON output and exit
code. This verifies the public interface, not internal helpers.
"""
import json
import subprocess
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
ADR_LINT = REPO_ROOT / "bin" / "adr-lint"
FIXTURES = REPO_ROOT / "tests" / "fixtures"


def run_lint(*args):
    """Invoke adr-lint with --format json and return (exit_code, parsed_json)."""
    result = subprocess.run(
        [sys.executable, str(ADR_LINT), "--format", "json", *args],
        capture_output=True, text=True, encoding="utf-8",
    )
    if not result.stdout.strip():
        return result.returncode, {"_stderr": result.stderr}
    try:
        return result.returncode, json.loads(result.stdout)
    except json.JSONDecodeError:
        return result.returncode, {"_stdout": result.stdout, "_stderr": result.stderr}


def test_canonical_pass():
    code, out = run_lint(str(FIXTURES / "canonical"))
    assert code == 0
    assert out["summary"] == {"pass": 1, "advisory": 0, "fail": 0, "skipped": 0, "total": 1}


def test_missing_headings_fails_default():
    code, out = run_lint(str(FIXTURES / "missing-headings"))
    assert code == 1
    assert out["summary"]["fail"] == 1
    fnd = out["files"][0]["findings"]
    assert any(f["gate"] == "completeness" and f["level"] == "FAIL" for f in fnd)


def test_bad_filename_consistency_fail():
    code, out = run_lint(str(FIXTURES / "bad-filename"))
    assert code == 1
    fnd = out["files"][0]["findings"]
    assert any(f["gate"] == "consistency" and f["level"] == "FAIL" for f in fnd)


def test_heading_mismatch_consistency_fail():
    code, out = run_lint(str(FIXTURES / "heading-mismatch"))
    assert code == 1
    fnd = out["files"][0]["findings"]
    assert any(f["gate"] == "consistency" and f["level"] == "FAIL" for f in fnd)


def test_marker_skip_whole_file():
    code, out = run_lint(str(FIXTURES / "marker-skip"))
    assert code == 0
    assert out["files"][0]["bucket"] == "SKIPPED"
    assert out["files"][0]["skip_reason"] == "marker"


def test_marker_advisory_demotes_failures():
    code, out = run_lint(str(FIXTURES / "marker-advisory"))
    assert code == 0  # ADVISORY only, no FAIL.
    assert out["summary"]["advisory"] == 1
    assert out["summary"]["fail"] == 0
    fnd = out["files"][0]["findings"]
    assert all(f["level"] == "ADVISORY" for f in fnd)


def test_marker_skip_gate_only():
    code, out = run_lint(str(FIXTURES / "marker-skip-gate"))
    # Completeness skipped; consistency still runs but should pass for this fixture.
    assert code == 0
    assert all(
        f["gate"] != "completeness" for f in out["files"][0]["findings"]
    )


def test_strict_from_boundary():
    """ADR-001 (legacy) should be ADVISORY, ADR-100 (recent) should be PASS."""
    code, out = run_lint(str(FIXTURES / "with-policy"))
    by_num = {f["adr_num"]: f for f in out["files"]}
    assert by_num[1]["bucket"] == "ADVISORY"
    assert by_num[100]["bucket"] == "PASS"
    assert code == 0


def test_strict_from_override_via_cli():
    """--strict-from on the command line overrides the config file."""
    # Override to ADR-001, making the legacy-shape file post-boundary -> FAIL.
    code, out = run_lint(
        "--strict-from", "ADR-001",
        str(FIXTURES / "with-policy"),
    )
    by_num = {f["adr_num"]: f for f in out["files"]}
    assert by_num[1]["bucket"] == "FAIL"
    assert code == 1


def test_gates_filter():
    """--gates limits which checks run."""
    code, out = run_lint(
        "--gates", "completeness",
        str(FIXTURES / "bad-filename"),
    )
    # Filename pattern would normally FAIL consistency, but --gates limits to completeness.
    assert code == 0  # ADR-003 has all canonical sections so completeness passes.
    fnd = out["files"][0]["findings"]
    assert all(f["gate"] == "completeness" for f in fnd)


def test_bad_config_exits_2():
    code, out = run_lint(str(FIXTURES / "bad-config"))
    assert code == 2


def test_unknown_gate_exits_2():
    code, out = run_lint("--gates", "fizzbuzz", str(FIXTURES / "canonical"))
    assert code == 2


def test_missing_path_exits_2():
    code, out = run_lint(str(FIXTURES / "this-does-not-exist"))
    assert code == 2


def test_single_file_lints():
    code, out = run_lint(str(FIXTURES / "canonical" / "ADR-001-clean-baseline.md"))
    assert code == 0
    assert out["summary"]["total"] == 1


def test_human_format_runs():
    """Human format produces non-JSON output and still exits cleanly."""
    result = subprocess.run(
        [sys.executable, str(ADR_LINT), "--format", "human", str(FIXTURES / "canonical")],
        capture_output=True, text=True, encoding="utf-8",
    )
    assert result.returncode == 0
    assert "PASS strictly (1)" in result.stdout
    assert "Aggregate:" in result.stdout
