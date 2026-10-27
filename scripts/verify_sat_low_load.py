#!/usr/bin/env python3
"""TASK-1411: report existing SAT tests and independent low-load contract probes.

Stdlib only. Compiles real firmware through the existing host-test adapters; it
does not flash, contact a device, change settings, or simulate boiler physics.
Baseline success means the report ran, never that the controller is accepted.
"""
from __future__ import annotations

import argparse
import concurrent.futures
import datetime as dt
import hashlib
import importlib
import io
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import time
import unittest

SUITES = (
    "test_sat_pwm_override", "test_sat_pwm_start_limit", "test_sat_hp_pwm",
    "test_sat_overshoot_sustain", "test_sat_pwm_replay_fixture",
)
PENDING = (
    ("LL-010", "Capability veto: MsgID 3 HB4, permitted/forbidden/unknown"),
    ("LL-011", "Per-ID freshness/invalidity while status replies continue"),
    ("LL-012", "DHW transitions and post-DHW recovery in the complete loop"),
    ("LL-013", "Queue full/retry/acknowledgement and actual wire ordering"),
    ("LL-014", "MaxTset limits throughout hold, refresh and recovery"),
    ("LL-015", "Heat-pump automatic entry and source-specific recovery"),
    ("LL-016", "Room control and measured heat/energy performance"),
    ("LL-017", "Overshoot-protection toggle and mode consistency"),
    ("LL-018", "No-return-temperature ignition and polling"),
    ("LL-019", "Phase anchors, start counts and duty ranges"),
    ("LL-020", "PID details, continuous clamp and independent scheduler integration"),
    ("LL-021", "Classifier inputs, rule order and mode-switch prerequisites"),
    ("LL-022", "Telemetry distinguishes intended, queued, acknowledged and measured"),
    ("LL-023", "Documentation/decision/task consistency and independent review"),
    ("PH-PIC", "PIC real-boiler timestamped wire and physical flame validation"),
    ("PH-OTD", "OT-Direct real-boiler timestamped wire and flame validation"),
    ("PH-FIELD", "Matched field comparison with measured heat/energy and uncertainty"),
    ("PH-PROFILE", "Approved equipment-specific ON/OFF/minimum-flow limits and targets"),
)


def run_process(argv: list[str], **kwargs) -> subprocess.CompletedProcess:
    return subprocess.run(argv, text=True, encoding="utf-8", errors="replace",
                          capture_output=True, check=True, **kwargs)


def fields(line: str) -> dict:
    return {k: float(v) for k, v in (part.split("=", 1) for part in line.split())}


def pwm_contract_satisfied(rows: list[dict], scenario: str) -> bool:
    """Assert phase AND CH; OFF with protection enabled must also send CS=10."""
    if len(rows) != 3 or [row.get("elapsed_s") for row in rows] != [0, 30, 60]:
        return False
    request = 28.568 if scenario == "ultra" else 38.6
    if any(abs(row.get("request", float("inf")) - request) > .001 for row in rows):
        return False
    if scenario == "ultra":
        return all(row["on_s"] == 0 and row["phase_on"] == 0 and row["ch"] == 0
                   and abs(row["cs"] - 10) <= .001 for row in rows)
    return all(row["phase_on"] == 1 and row["ch"] == 1 for row in rows)


def compile_run(compiler, folder: Path, source: str, argv=()) -> list[dict]:
    folder.mkdir()
    cpp = folder / "probe.cpp"
    cpp.write_text(source, encoding="utf-8")
    exe = compiler(cpp, folder / "probe.exe")
    if exe is None:
        raise RuntimeError("No host C++ compiler: g++, clang++ or Visual Studio C++ required")
    return [fields(line) for line in run_process([str(exe), *argv], timeout=30).stdout.splitlines() if line]


def probe_contracts(repo: Path) -> list[dict]:
    """Only stimuli/assertions live here; the controller comes from .ino sources."""
    sys.path.insert(0, str(repo / "tests"))
    hp = importlib.import_module("test_sat_hp_pwm")
    sustain = importlib.import_module("test_sat_overshoot_sustain")
    base = importlib.import_module("test_sat_pwm_override")
    results = []
    with tempfile.TemporaryDirectory(prefix="sat-low-load-") as temporary:
        work = Path(temporary)
        # Reuse the existing real-code extraction, changing only its test main.
        old_main = hp.MAIN
        hp.MAIN = hp.MAIN[:hp.MAIN.index("int main")] + r'''
int main(int argc, char** argv) {
  reset(); g_source=SAT_SRC_GAS_BOILER; settings.sat.iCyclesPerHour=3;
  g_flame=false; g_flameOnStart=g_ms-4000000UL; g_flameOffStart=g_ms-3000000UL;
  const bool ultra = argc > 1 && !strcmp(argv[1], "ultra");
  const float request=ultra ? 28.568f : 38.6f;
  for(int i=0;i<3;i++) {
    const float cs=satApplyPWM(request);
    const unsigned ch=satChEnableDecide(request,g_cold,satPwmDrivesSetpoint(request),state.sat.bPwmFlameRequested);
    printf("elapsed_s=%d request=%.3f duty=%.6f on_s=%.3f off_s=%.3f phase_on=%d cs=%.3f ch=%u\n",
      i*30,request,state.sat.fPwmDutyCycle,_pwm_lastOnTimeMs/1000.0,_pwm_lastOffTimeMs/1000.0,
      (int)state.sat.bPwmFlameRequested,cs,ch);
    g_ms+=30000UL;
  }
}
'''
        try:
            folder = work / "pwm"
            folder.mkdir()
            exe = hp.build(folder)
            if exe is None:
                raise RuntimeError("No host C++ compiler")
            for ident, scenario in (("LL-001", "ultra"), ("LL-002", "slow")):
                rows = [fields(line) for line in run_process([str(exe), scenario], timeout=30).stdout.splitlines()]
                okay = pwm_contract_satisfied(rows, scenario)
                results.append({"id": ident, "status": "pass" if okay else "fail", "observed": rows,
                    "expected": "Mapped on_time=0: phase OFF, CH=0 and CS=10 (protection enabled)" if scenario == "ultra" else
                                "Keep phase ON and CH=1 while awaiting ignition during the first 60 s",
                    "scope": "Real PWM block + CH helper, stubbed clock/sensors/queue; no physical boiler",
                    "authority": "TASK-1383/Q38 candidate" if scenario == "ultra" else "TASK-1382/Q35 A candidate"})
        finally:
            hp.MAIN = old_main

        # Compile the COMPLETE SATpid.ino, including its own update guards.
        pid_prelude = r'''
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#define F(x) x
#define PSTR(x) x
#define SATDebugTf(...) ((void)0)
#define DebugTln(...) ((void)0)
enum { SAT_HSYS_RADIATORS=1, SAT_HSYS_UNDERFLOOR=2 };
struct Settings { struct { bool bAutoGains=false,bSolarFreezeIntegral=false;
 float fKpManual=0,fKiManual=.01f,fKdManual=0,fHeatingCurveCoeff=2,fDeadband=.1f; } sat; } settings;
struct State { struct { bool bSolarGainActive=false;
 float fPidP=0,fPidI=0,fPidD=0,fPidOutput=0,fKp=0,fKi=0,fKd=0,fRawDerivative=0,fError=0; } sat; } state;
static uint32_t g_ms=1000000;
static uint32_t millis(){return g_ms;}
static int satGetEffectiveHeatingSystem(){return SAT_HSYS_RADIATORS;}
'''
        pid_main = r'''
int main(int argc,char** argv) {
 const int interval=atoi(argv[1]); const bool changing=atoi(argv[2])!=0;
 satPidReset(); satPidUpdate(20.95f,21,40,45);
 for(int elapsed=interval;elapsed<=600;elapsed+=interval) {
   g_ms=1000000UL+elapsed*1000UL;
   const float error=.05f+(changing && ((elapsed/30)%2) ? .01f : 0.0f);
   satPidUpdate(21-error,21,40,45);
 }
 printf("interval_s=%d changing=%d integral=%.6f\n",interval,(int)changing,state.sat.fPidI);
}
'''
        pid_source = pid_prelude + (repo / "src/OTGW-firmware/SATpid.ino").read_text(encoding="utf-8") + pid_main
        rows = compile_run(base.compile_cpp, work / "pid", pid_source, ("60", "0"))
        pid_exe = work / "pid/probe.exe"
        for changing in (0, 1):
            for interval in (10, 30, 60, 120):
                if changing == 0 and interval == 60:
                    continue
                rows.extend(fields(line) for line in run_process([str(pid_exe), str(interval), str(changing)], timeout=30).stdout.splitlines())
        references = {changing: next(row["integral"] for row in rows
                      if row["interval_s"] == 60 and row["changing"] == changing) for changing in (0, 1)}
        okay = all(abs(row["integral"] - references[row["changing"]]) <= .0001 for row in rows)
        results.append({"id": "LL-003", "status": "pass" if okay else "fail", "observed": rows,
            "expected": "Same 600 s physical error trace: integral independent of control tick (60 s reference)",
            "scope": "Whole SATpid.ino under the current shared-tick call contract; scheduler integration remains LL-020",
            "authority": "Sergeant D entry 42 / TASK-1368; adapter must be reviewed when PID scheduling is separated"})

        # Reuse the actual sustain-state/function extraction; assert wall time
        # since the FIRST qualifying sample, rather than preserving old timing.
        source = (repo / "src/OTGW-firmware/SATcycles.ino").read_text(encoding="utf-8").replace("\r\n", "\n")
        flag, body = sustain.extract(source)
        compile_run(base.compile_cpp, work / "sustain", flag + sustain.STUBS + body + sustain.MAIN,
                    tuple(sustain.ticks(30000, "u" * 30)))
        sustain_exe = work / "sustain/probe.exe"
        cases = []
        for interval in (30, 60, 90, 120, 300):
            for kind, threshold in (("u", 180), ("z", 300)):
                observed = fields(run_process([str(sustain_exe), *sustain.ticks(interval * 1000, kind * 30)], timeout=30).stdout.strip())
                elapsed = observed["t"] / 1000 - interval if observed["t"] >= 0 else None
                cases.append({"interval_s": interval, "condition": "underheat" if kind == "u" else "saturation",
                    "threshold_s": threshold, "qualifying_elapsed_s": elapsed, "observed": observed,
                    "pass": elapsed is not None and threshold <= elapsed <= threshold + interval and observed["mode"] == 1})
        results.append({"id": "LL-004", "status": "pass" if all(row["pass"] for row in cases) else "fail",
            "observed": cases, "expected": "Exit after 180/300 s from first qualifying sample at every supported tick",
            "scope": "Real satCycleCheckAutoSwitch, stubbed sensors/clock; full loop entry/recovery remains LL-020",
            "authority": "Q44 A candidate; TASK-1367 only repaired the opposite direction"})
    return results


def worker(kind: str, repo: Path, result_path: Path) -> int:
    payload = {}
    try:
        sys.path.insert(0, str(repo / "tests"))
        if kind == "contracts":
            payload = {"contracts": probe_contracts(repo)}
        else:
            stream = io.StringIO()
            result = unittest.TextTestRunner(stream=stream, verbosity=1).run(unittest.defaultTestLoader.loadTestsFromName(kind))
            payload = {"module": kind, "testsRun": result.testsRun, "failures": len(result.failures),
                "errors": len(result.errors), "skipped": len(result.skipped), "expectedFailures": len(result.expectedFailures),
                "unexpectedSuccesses": len(result.unexpectedSuccesses), "details": stream.getvalue(),
                "skips": [{"test": str(test), "reason": reason} for test, reason in result.skipped],
                "expectedFailureTests": [str(test) for test, _ in result.expectedFailures]}
    except Exception as exc:
        payload = {"infrastructureError": f"{type(exc).__name__}: {exc}"}
    result_path.write_text(json.dumps(payload, indent=2), encoding="utf-8")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--mode", choices=("baseline", "release"), default="baseline",
                        help="baseline records known gaps; release fails on every gap/xfail/skip (default: baseline)")
    parser.add_argument("--only", choices=("all", "suites", "contracts"), default="all",
                        help="diagnostic subset; omitted checks always block acceptance")
    parser.add_argument("--repo", type=Path, default=Path(__file__).resolve().parent.parent)
    parser.add_argument("--output", type=Path, help="JSON report path; otherwise JSON is written to stdout")
    parser.add_argument("--timeout", type=int, default=240, help="timeout seconds per isolated worker (default: 240)")
    args = parser.parse_args()
    if args.timeout < 1:
        parser.error("--timeout must be positive")
    repo = args.repo.resolve()
    started = time.monotonic()
    def git(*command):
        return run_process(["git", "-C", str(repo), *command], timeout=30).stdout.strip()
    metadata = {"startedUtc": dt.datetime.now(dt.timezone.utc).isoformat(), "python": sys.version,
                "repo": str(repo), "mode": args.mode, "selection": args.only}
    try:
        metadata.update(commit=git("rev-parse", "HEAD"), branch=git("branch", "--show-current"),
                        dirty=git("status", "--porcelain=v1").splitlines())
    except Exception as exc:
        parser.error(f"Cannot identify repository: {exc}")
    paths = ["src/OTGW-firmware/" + name for name in ("SATcontrol.ino", "SATpid.ino", "SATcycles.ino", "SATchEnable.h", "SATtypes.h", "settingStuff.ino")]
    paths += ["tests/" + name + ".py" for name in SUITES]
    paths += ["tests/test_sat_cycle_classifier.py", "scripts/gen_sat_pwm_replay.py",
              "tests/webui/fixtures/sat-pwm-suppression-replay.txt"]
    metadata["sha256"] = {path: hashlib.sha256((repo / path).read_bytes()).hexdigest() for path in paths}
    runner_path = Path(__file__).resolve()
    metadata["runner"] = {"path": str(runner_path), "sha256": hashlib.sha256(runner_path.read_bytes()).hexdigest()}
    metadata["hashScope"] = "Selected target firmware, test adapters and replay inputs; runner is the separately identified executing file"
    overrides = {key: value for key, value in os.environ.items() if key.startswith("OTGW_SAT") or key == "OTGW_SETTINGS_INO"}
    metadata["clearedSourceOverrideNames"] = sorted(overrides)
    environment = dict(os.environ, PYTHONDONTWRITEBYTECODE="1", PYTHONUTF8="1")
    for key in overrides:
        environment.pop(key, None)
    jobs = list(SUITES) if args.only != "contracts" else []
    if args.only != "suites":
        jobs.append("contracts")
    with tempfile.TemporaryDirectory(prefix="sat-low-load-report-") as temporary:
        def execute(name):
            result_path = Path(temporary) / (name + ".json")
            try:
                output = run_process([sys.executable, str(Path(__file__).resolve()), "--worker", name,
                                      str(repo), str(result_path)], env=environment, cwd=repo, timeout=args.timeout)
                result = json.loads(result_path.read_text(encoding="utf-8"))
                if output.stdout or output.stderr:
                    result["workerLogTail"] = (output.stdout + output.stderr)[-4000:]
                return name, result
            except Exception as exc:
                return name, {"infrastructureError": f"{type(exc).__name__}: {exc}"}
        with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
            outcomes = dict(pool.map(execute, jobs))
    suites = [outcomes[name] for name in SUITES if name in outcomes]
    contracts = outcomes.get("contracts", {}).get("contracts", [])
    blockers = [f"{ident}: unavailable - {description}" for ident, description in PENDING]
    if args.only != "all":
        blockers.append(f"Subset run ({args.only}); omitted checks are unavailable")
    infrastructure = [f"{name}: {row['infrastructureError']}" for name, row in outcomes.items() if "infrastructureError" in row]
    regressions = []
    for suite in suites:
        if "infrastructureError" in suite:
            continue
        for key in ("failures", "errors", "unexpectedSuccesses"):
            if suite[key]:
                regressions.append(f"{suite['module']}: {key}={suite[key]}")
        for key in ("skipped", "expectedFailures"):
            if suite[key]:
                blockers.append(f"{suite['module']}: {key}={suite[key]}")
    blockers += [f"{row['id']}: {row['status']}" for row in contracts if row["status"] != "pass"]
    blockers += infrastructure + regressions
    totals = {key: sum(row.get(key, 0) for row in suites) for key in
              ("testsRun", "failures", "errors", "skipped", "expectedFailures", "unexpectedSuccesses")}
    report = {"schemaVersion": 1, "metadata": metadata, "suites": suites, "suiteTotals": totals,
        "contracts": contracts, "missingChecks": [{"id": ident, "status": "unavailable", "description": description} for ident, description in PENDING],
        "verdict": "blocked" if blockers else "accepted", "blockers": blockers,
        "baselineExecution": "error" if infrastructure else "regression" if regressions else "complete",
        "evidenceLevel": "Host control-path tests only; no physical boiler/efficiency acceptance",
        "elapsedSeconds": round(time.monotonic() - started, 3)}
    encoded = json.dumps(report, indent=2, ensure_ascii=False) + "\n"
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(encoded, encoding="utf-8")
    else:
        print(encoded, end="")
    print(f"SAT low-load: {report['baselineExecution']}; acceptance={report['verdict']}; tests={totals['testsRun']}; "
          f"xfail={totals['expectedFailures']}; blockers={len(blockers)}", file=sys.stderr)
    if infrastructure:
        return 2
    return 1 if regressions or (args.mode == "release" and blockers) else 0


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--worker":
        raise SystemExit(worker(sys.argv[2], Path(sys.argv[3]), Path(sys.argv[4])))
    raise SystemExit(main())
