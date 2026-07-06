"""
heap_soak_driver.py - TASK-956 representative-load heap-fragmentation soak
=============================================================================

PURPOSE
-------
Drives a REPRESENTATIVE (not adversarial) load profile against a dedicated
ESP32-S3 for an extended window, so the TASK-934 heap-diagnostics counters
(hd_min_max_block, hd_max_loop_gap_ms, hd_rest_503/webfile_503, enter_low/
warning/critical) can be read back and checked against TASK-956's proof
criterion for whether dev's preventive heap-frag gating is still needed on
ESP32-S3.

This is deliberately NOT an overload ramp (that's scripts/loadtest_harness.py/
loadtest_sweep.py, already used for TASK-989/1022/1024) - it mimics normal
day-to-day usage: occasional page loads, periodic API polling, background OT
traffic, sustained for hours rather than seconds.

ADAPTATIONS FROM THE ORIGINAL PROCEDURE (bench constraints, documented for
whoever reads the soak verdict):
  - sat_boiler_emulator.py needs an OTDirect TCP bridge (port 25238), which
    only exists on OTDirect boards (esp32-otgw32). This bench unit is
    esp32-classic (PIC), so OT traffic is generated via the firmware's own
    onboard "OTGW serial-simulation replay" (telnet 's', state.debug.bOTGWSimulation)
    instead - a real firmware feature, not a synthetic substitute.
  - MQTT discovery republish is not exercised: no MQTT broker was reachable
    from this environment during the soak window. This soak therefore isolates
    heap-frag behavior from Web UI + REST + OT traffic only, not the full
    MQTT-inclusive profile the original procedure describes.

USAGE
  python scripts/heap_soak_driver.py --host 192.168.88.64 --duration-hours 2
"""

import argparse
import json
import random
import socket
import sys
import time
import urllib.request

ASSET_PATHS = ["/v2.html", "/v2.css", "/ds-tokens.css", "/v2.js"]
API_PATHS = ["/api/v2/device/info", "/api/v2/sat/status", "/api/v2/health"]


def telnet_send(host, key, timeout=5.0):
    try:
        with socket.create_connection((host, 23), timeout=timeout) as s:
            s.settimeout(timeout)
            try:
                s.recv(4096)
            except OSError:
                pass
            s.sendall(key.encode())
            time.sleep(0.3)
        return True
    except OSError:
        return False


def fetch(host, path, timeout=5.0):
    try:
        with urllib.request.urlopen(f"http://{host}{path}", timeout=timeout) as r:
            return r.status
    except Exception:
        return None


def fetch_device_info(host, timeout=5.0):
    try:
        with urllib.request.urlopen(f"http://{host}/api/v2/device/info", timeout=timeout) as r:
            return json.loads(r.read().decode()).get("device", {})
    except Exception:
        return None


def hd_snapshot(info):
    if not info:
        return None
    keys = ["freeheap", "maxfreeblock", "hd_min_max_block", "hd_min_free_heap",
            "hd_max_loop_gap_ms", "hd_enter_low", "hd_enter_warning", "hd_enter_critical",
            "hd_drip_slowmode", "hd_ws_drops", "hd_mqtt_drops", "hd_rest_503", "hd_webfile_503"]
    return {k: info.get(k) for k in keys}


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--host", required=True)
    p.add_argument("--duration-hours", type=float, default=2.0)
    p.add_argument("--poll-interval-sec", type=float, default=20.0, help="Representative-load request cadence")
    p.add_argument("--snapshot-log", default="build/heap_soak_snapshots.ndjson")
    args = p.parse_args()

    print(f"Resetting heap-diag counters from a healthy heap (telnet 'z')...", file=sys.stderr)
    if not telnet_send(args.host, "z"):
        print("WARNING: could not reach telnet to reset counters", file=sys.stderr)
    baseline = hd_snapshot(fetch_device_info(args.host))
    print("baseline:", json.dumps(baseline), file=sys.stderr)

    print("Enabling onboard OTGW serial-simulation replay for continuous OT traffic...", file=sys.stderr)
    telnet_send(args.host, "s")

    deadline = time.monotonic() + args.duration_hours * 3600
    log_fh = open(args.snapshot_log, "w", encoding="utf-8")
    log_fh.write(json.dumps({"ts": time.time(), "phase": "baseline", **(baseline or {})}) + "\n")
    log_fh.flush()

    request_count = 0
    try:
        while time.monotonic() < deadline:
            # Representative mix: mostly API polls (dashboard refresh pattern),
            # occasional full asset reload (user navigating/reloading the page).
            if random.random() < 0.15:
                for path in ASSET_PATHS:
                    fetch(args.host, path)
                    request_count += 1
                    time.sleep(0.3)  # sequential, mimics single-flight client behavior
            else:
                path = random.choice(API_PATHS)
                fetch(args.host, path)
                request_count += 1

            time.sleep(args.poll_interval_sec)

            info = fetch_device_info(args.host)
            snap = hd_snapshot(info)
            if snap:
                remaining_min = round((deadline - time.monotonic()) / 60, 1)
                log_fh.write(json.dumps({"ts": time.time(), "phase": "running", "requests_so_far": request_count, **snap}) + "\n")
                log_fh.flush()
                print(f"[{remaining_min}min left] requests={request_count} freeheap={snap['freeheap']} "
                      f"maxblock={snap['maxfreeblock']} hd_min_max_block={snap['hd_min_max_block']} "
                      f"enter_warn={snap['hd_enter_warning']} enter_crit={snap['hd_enter_critical']} "
                      f"drip_slow={snap['hd_drip_slowmode']}", file=sys.stderr)
            else:
                print("WARNING: device/info fetch failed during soak (telnet/HTTP unreachable?)", file=sys.stderr)
                log_fh.write(json.dumps({"ts": time.time(), "phase": "unreachable"}) + "\n")
                log_fh.flush()
    except KeyboardInterrupt:
        print("Interrupted by user.", file=sys.stderr)

    print("Disabling onboard OT simulation...", file=sys.stderr)
    telnet_send(args.host, "s")

    final = hd_snapshot(fetch_device_info(args.host))
    log_fh.write(json.dumps({"ts": time.time(), "phase": "final", "requests_total": request_count, **(final or {})}) + "\n")
    log_fh.close()

    print(json.dumps({"baseline": baseline, "final": final, "requests_total": request_count}, indent=2))


if __name__ == "__main__":
    main()
