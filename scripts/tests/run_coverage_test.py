#!/usr/bin/env python3
"""Run the OT coverage fixture against a device and check it against the baseline.

One command instead of the five-step manual sequence (upload, start, capture,
stop, compare). Exits 0 when the run matches the committed baseline and non-zero
on drift or on any device error, so it works as a gate after a firmware change.

    python run_coverage_test.py --host otgw1.local

The simulation is stopped in a finally block: a failed capture or a drifting
comparison must never leave a bench publishing synthetic OpenTherm data to a real
MQTT broker.

Capture length defaults to one full fixture loop plus a margin. A capture cut
mid-loop reports topics as MISSING purely because they had not come round yet,
which looks exactly like a regression, so the runner refuses to run short.
"""
from __future__ import annotations

import argparse
import os
import socket
import sys
import time
import urllib.error
import urllib.request

import coverage_baseline

HERE = os.path.dirname(os.path.abspath(__file__))
FIXTURE = os.path.join(HERE, "otgw_simulation_coverage.log")
BASELINE = os.path.join(HERE, "baseline_coverage.json")
FRAME_INTERVAL_S = 0.75          # device default, /api/v2/simulate reports it
TELNET_PORT = 23


def http(url: str, method: str = "GET", timeout: int = 15) -> str:
    req = urllib.request.Request(url, method=method)
    with urllib.request.urlopen(req, timeout=timeout) as resp:
        return resp.read().decode("utf-8", "replace")


def upload_fixture(host: str, path: str) -> None:
    """Multipart upload to /upload, keeping the device-side filename."""
    boundary = "----otgwcoverage"
    body = (
        f"--{boundary}\r\n"
        f'Content-Disposition: form-data; name="file"; filename="otgw_simulation.log"\r\n'
        f"Content-Type: application/octet-stream\r\n\r\n"
    ).encode() + open(path, "rb").read() + f"\r\n--{boundary}--\r\n".encode()
    req = urllib.request.Request(
        f"http://{host}/upload", data=body, method="POST",
        headers={"Content-Type": f"multipart/form-data; boundary={boundary}"},
    )
    try:
        urllib.request.urlopen(req, timeout=60).read()
    except urllib.error.HTTPError as exc:
        if exc.code not in (200, 302, 303):     # the handler answers 303 on success
            raise


def capture(host: str, seconds: int, out_path: str) -> int:
    """Log telnet:23, enabling MQTT debug only if the banner shows it is off.

    The debug keys are toggles, not setters: pressing '3' on an already-enabled
    session turns MQTT debug back OFF and the capture silently loses every
    publish line.
    """
    sock = socket.create_connection((host, TELNET_PORT), timeout=10)
    sock.settimeout(1.0)
    banner = ""
    deadline = time.time() + 4
    while time.time() < deadline:
        try:
            banner += sock.recv(4096).decode("utf-8", "replace")
        except socket.timeout:
            pass
    import re
    for key, name in (("3", "MQTT"), ("4", "MQTTGate")):
        m = re.search(re.escape(key) + r"\s+" + name + r"\s*\[(\d)\]", banner)
        if m and m.group(1) == "0":
            sock.sendall(key.encode())
            time.sleep(0.6)

    lines = 0
    with open(out_path, "w", encoding="utf-8", errors="replace") as fh:
        stop = time.time() + seconds
        while time.time() < stop:
            try:
                data = sock.recv(4096)
                if not data:
                    break
                text = data.decode("utf-8", "replace")
                fh.write(text)
                fh.flush()
                lines += text.count("\n")
            except socket.timeout:
                continue
    sock.close()
    return lines


def main() -> int:
    fixture_frames = sum(1 for _ in open(FIXTURE, encoding="ascii"))
    loop_s = int(fixture_frames * FRAME_INTERVAL_S)
    default_s = loop_s + 60

    ap = argparse.ArgumentParser()
    ap.add_argument("--host", required=True)
    ap.add_argument("--seconds", type=int, default=default_s,
                    help=f"capture length (default {default_s}s = one {loop_s}s loop + margin)")
    ap.add_argument("--baseline", default=BASELINE)
    ap.add_argument("--out", default=os.path.join(HERE, "last-coverage-run.log"))
    ap.add_argument("--keep-running", action="store_true",
                    help="leave the simulation enabled after the run (default: stop it)")
    args = ap.parse_args()

    if args.seconds < loop_s:
        print(f"refusing to run: {args.seconds}s is shorter than one {loop_s}s fixture loop.\n"
              f"A partial loop reports topics as MISSING that simply had not come round yet,\n"
              f"which is indistinguishable from a real regression.", file=sys.stderr)
        return 2

    print(f"device   : {args.host}")
    print(f"fixture  : {fixture_frames} frames, loop {loop_s}s")
    print(f"capture  : {args.seconds}s -> {args.out}")

    started = False
    try:
        upload_fixture(args.host, FIXTURE)
        print("uploaded : fixture -> /otgw_simulation.log")
        http(f"http://{args.host}/api/v2/simulate/start", method="POST")
        started = True
        print("started  : simulation running")

        lines = capture(args.host, args.seconds, args.out)
        print(f"captured : {lines} lines")
    except (OSError, urllib.error.URLError) as exc:
        print(f"device error: {exc}", file=sys.stderr)
        return 2
    finally:
        if started and not args.keep_running:
            try:
                http(f"http://{args.host}/api/v2/simulate/stop", method="POST")
                print("stopped  : simulation disabled")
            except Exception as exc:                      # noqa: BLE001 - best effort
                print(f"WARNING: could not stop simulation: {exc}", file=sys.stderr)

    import json
    with open(args.baseline, encoding="utf-8") as fh:
        base = json.load(fh)
    current = coverage_baseline.fingerprint(coverage_baseline.read(args.out))
    problems = coverage_baseline.diff(base, current)

    counts = current["counts"]
    print(f"decoded  : {counts['ot_keys']} keys, {counts['msgids']} MsgIDs, "
          f"{counts['mqtt_topics']} topics")

    if not problems:
        print("\nPASS: run matches the baseline")
        return 0

    print(f"\nFAIL: {len(problems)} difference(s) vs baseline\n")
    for p in problems:
        print(p)
    print(f"\nCapture kept at {args.out} for investigation.")
    print("If the change is intended, review the diff and refresh with:")
    print(f"  python coverage_baseline.py record {args.out}")
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
