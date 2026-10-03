#!/usr/bin/env python3
"""TASK-1195 bench check: does the SAT PID derivative follow a stepped room temperature?

    python scripts/tests/sat_derivative_bench.py --host <ip> --label <name> --out <file.jsonl>

Preconditions on the device (save GET /api/v2/settings first and restore it after):
SAT enabled (POST /api/v2/sat/enable/1), satexternaltemp true, a single zone, and
nothing that outranks the external temperature (BLE, multi-area). No boiler is needed.

The script feeds the external room temperature, 18.00 C and then +0.1 C every 150 s
(16 steps), through POST /api/v2/sat/externaltemp/<v>. It polls GET /api/v2/sat/status
every 15 s, one request at a time (the REST gate caps in-flight calls at 2). On the
device the room EMA (TASK-894, tau 45 s) smooths every step before satPidUpdate(), so
this measures the derivative on the real device path.

Output: one JSON line per feed and per poll, then a summary line with the mean
raw_derivative over the polls of steps 9-16 and its ratio to the true slope
(-0.1/150 = -0.000667 per s).

Reference results (TASK-1195, OTGW32):
- alpha.402 (old timer): -0.000262, 39%;
- alpha.404 (fix): -0.000676, 101%.
The host harness test/host/test_sat_pid_derivative.cpp predicts 25-49% and 92-98%
for this scenario. Exit code: 0 when the run completed, whatever the ratio.
"""
import argparse
import json
import time
import urllib.request

START, STEP, PERIOD, N, POLL = 18.0, 0.1, 150.0, 16, 15.0


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--host", required=True, help="device IP; never a default host")
    ap.add_argument("--label", default="run", help="label stored in the summary")
    ap.add_argument("--out", required=True, help="JSON-lines output file")
    args = ap.parse_args()

    def req(method, path):
        r = urllib.request.Request(f"http://{args.host}{path}", method=method)
        with urllib.request.urlopen(r, timeout=10) as f:
            return f.read().decode()

    log = open(args.out, "w", encoding="utf-8")
    samples = []
    t0 = time.monotonic()
    next_step, next_poll, end = 0, t0, t0 + (N + 1) * PERIOD
    while True:
        now = time.monotonic()
        if next_step <= N and now >= t0 + next_step * PERIOD:
            v = START + STEP * next_step
            try:
                req("POST", f"/api/v2/sat/externaltemp/{v:.2f}")
                log.write(json.dumps({"t": round(now - t0, 1), "feed": round(v, 2)}) + "\n")
            except Exception as e:  # noqa: BLE001 - log and keep the schedule
                log.write(json.dumps({"t": round(now - t0, 1), "feed_error": str(e)}) + "\n")
            log.flush()
            next_step += 1
        if now >= next_poll:
            try:
                d = json.loads(req("GET", "/api/v2/sat/status"))
                rec = {"t": round(now - t0, 1), "step": next_step - 1, "room": d.get("room_temp"),
                       "raw": d.get("raw_derivative"), "error": d.get("error"), "enabled": d.get("enabled"),
                       "mode": d.get("control_mode"), "tripped": d.get("safety_tripped")}
            except Exception as e:  # noqa: BLE001
                rec = {"t": round(now - t0, 1), "poll_error": str(e)}
            log.write(json.dumps(rec) + "\n")
            log.flush()
            samples.append(rec)
            next_poll += POLL
        if now >= end:
            break
        time.sleep(1.0)

    late = [s["raw"] for s in samples if s.get("step", -1) >= N // 2 + 1 and isinstance(s.get("raw"), (int, float))]
    slope = -STEP / PERIOD
    mean = sum(late) / len(late) if late else float("nan")
    summary = {
        "label": args.label, "polls": len(samples), "late_polls": len(late),
        "mean_raw_steps_9_16": round(mean, 6), "true_slope": round(slope, 6),
        "ratio_to_slope": round(mean / slope, 3) if late else None,
        "ever_tripped": any(s.get("tripped") for s in samples),
        "always_enabled": all(s.get("enabled") for s in samples if "enabled" in s),
        "poll_errors": sum(1 for s in samples if "poll_error" in s),
    }
    log.write(json.dumps({"summary": summary}) + "\n")
    log.close()
    print(json.dumps(summary))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
