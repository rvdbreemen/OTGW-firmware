"""
loadtest_sweep.py - Phase 1 capacity-curve sweep driver (TASK-1022)
======================================================================

PURPOSE
-------
Implements the Phase 1 method from docs/superpowers/specs/2026-07-06-parallelism-loadtest-design.md:
ONE instrumented firmware build with a bounded safety gate (REST_MAX_INFLIGHT=8,
WEB_FILE_MAX_INFLIGHT=8 - above every test-N so a true runaway still sheds and the
device can never be bricked), then sweep offered-concurrency in SOFTWARE (no reflash
between steps) across N in {1,2,3,4,6,8}, running scenarios S1-S5 per arm:

  S1 cold page-load          -> scripts/loadtest_pageload.py (cold profile)
  S2 warm page-load          -> scripts/loadtest_pageload.py (primed profile)
  S3 API burst               -> scripts/loadtest_harness.py at concurrency=N
  S4 combined-stress         -> scripts/loadtest_combined.py at api-concurrency=N
  S5 overload (offer 2N)     -> scripts/loadtest_harness.py at concurrency=2*N
     (exercises the 503-shed + recovery path; still bounded by the gate cap=8)

Each arm runs under scripts/loadtest_arm_runner.py (telnet 'z' reset, one merged
capture-mqtt-debug.bat transcript, concurrent safety watchdog) so an incident on
any arm aborts that arm's capture and is recorded, without needing a human present.

OUTPUT
------
One JSON row per arm with: page-load p50-ish (single-sample, this is a smoke-scale
sweep not a statistical study), 503 counts, firmware hwm snapshot (hd_* fields from
/api/v2/device/info), and the watchdog incident list. Final summary picks
N* = highest offered-N with zero incidents AND hd_rest_503/hd_webfile_503 == 0.

USAGE
-----
  python scripts/loadtest_sweep.py --host 192.168.88.64 --scenario-seconds 10
"""

import argparse
import json
import subprocess
import sys
import time
import urllib.request

SWEEP_N = [1, 2, 3, 4, 6, 8]


def fetch_device_info(host, timeout=5.0):
    try:
        with urllib.request.urlopen(f"http://{host}/api/v2/device/info", timeout=timeout) as r:
            return json.loads(r.read().decode())
    except Exception as exc:
        return {"_fetch_error": str(exc)}


def hwm_snapshot(info):
    # /api/v2/device/info nests everything under a top-level "device" object.
    device = info.get("device", {}) if isinstance(info, dict) else {}
    keys = [
        "hd_min_max_block", "hd_min_free_heap", "hd_max_loop_gap_ms",
        "hd_rest_inflight_hwm", "hd_webfile_inflight_hwm", "hd_tcp_active_pcbs",
        "hd_rest_503", "hd_webfile_503",
    ]
    return {k: device.get(k) for k in keys}


def run_scenario(cmd, timeout):
    try:
        proc = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout)
        return {"returncode": proc.returncode, "stdout_tail": proc.stdout[-2500:], "stderr_tail": proc.stderr[-500:]}
    except subprocess.TimeoutExpired:
        return {"returncode": None, "error": "scenario timed out"}


def run_arm(host, n, scenario_seconds, arm_duration, output_root):
    print(f"=== arm N={n} ===", file=sys.stderr)

    arm_runner = subprocess.Popen(
        [sys.executable, "scripts/loadtest_arm_runner.py",
         "--host", host, "--arm-name", f"N{n}", "--duration", str(arm_duration),
         "--output-root", output_root],
        stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True,
    )

    # Give the arm_runner a moment to send its telnet 'z' reset before scenarios start.
    time.sleep(2)

    scenarios = {}
    scenarios["S1_S2_pageload"] = run_scenario(
        [sys.executable, "scripts/loadtest_pageload.py", "--host", host], timeout=90)
    scenarios["S3_api_burst"] = run_scenario(
        [sys.executable, "scripts/loadtest_harness.py", "--host", host,
         "--concurrency", str(n), "--duration", str(scenario_seconds)], timeout=scenario_seconds + 30)
    scenarios["S4_combined"] = run_scenario(
        [sys.executable, "scripts/loadtest_combined.py", "--host", host,
         "--duration", str(scenario_seconds), "--api-concurrency", str(n)], timeout=scenario_seconds + 60)
    scenarios["S5_overload_2N"] = run_scenario(
        [sys.executable, "scripts/loadtest_harness.py", "--host", host,
         "--concurrency", str(2 * n), "--duration", str(scenario_seconds)], timeout=scenario_seconds + 30)

    try:
        arm_stdout, arm_stderr = arm_runner.communicate(timeout=max(10, arm_duration))
    except subprocess.TimeoutExpired:
        arm_runner.kill()
        arm_stdout, arm_stderr = arm_runner.communicate(timeout=10)

    try:
        arm_result = json.loads(arm_stdout)
    except (ValueError, TypeError):
        arm_result = {"_parse_error": arm_stdout, "_stderr": arm_stderr}

    info = fetch_device_info(host)
    return {
        "offered_n": n,
        "arm_capture": arm_result,
        "hwm_snapshot": hwm_snapshot(info),
        "scenarios": scenarios,
    }


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--host", required=True, help="Device IP, e.g. 192.168.88.64")
    p.add_argument("--scenario-seconds", type=float, default=10.0, help="Duration for each of S3/S4/S5 per arm")
    p.add_argument("--arm-duration", type=float, default=None, help="capture-mqtt-debug.bat window per arm (default: auto, covers all scenarios + margin)")
    p.add_argument("--output-root", default="logs/loadtest-sweep-phase1", help="Root dir for per-arm transcripts")
    p.add_argument("--sweep", default=None, help="Comma-separated offered-N list (default: 1,2,3,4,6,8)")
    args = p.parse_args()

    sweep = [int(x) for x in args.sweep.split(",")] if args.sweep else SWEEP_N
    arm_duration = args.arm_duration or (args.scenario_seconds * 3 + 60)

    rows = []
    for n in sweep:
        rows.append(run_arm(args.host, n, args.scenario_seconds, arm_duration, args.output_root))

    n_star = None
    for row in reversed(rows):
        hwm = row["hwm_snapshot"]
        zero_503 = (hwm.get("hd_rest_503") == 0 and hwm.get("hd_webfile_503") == 0)
        zero_incidents = not row["arm_capture"].get("incidents")
        not_aborted = not row["arm_capture"].get("aborted", True)
        if zero_503 and zero_incidents and not_aborted:
            n_star = row["offered_n"]
            break

    summary = {"sweep": sweep, "n_star": n_star, "arms": rows}
    print(json.dumps(summary, indent=2, default=str))


if __name__ == "__main__":
    main()
