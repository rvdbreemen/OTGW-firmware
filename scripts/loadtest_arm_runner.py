"""
loadtest_arm_runner.py - per-arm capture + safety watchdog for TASK-1015 (TASK-1021)
=======================================================================================

PURPOSE
-------
Wraps one "arm" of the parallelism-optimum sweep (one fixed offered-N value) with:

  1. A telnet 'z' reset of the on-device heap/hwm/counter diagnostics before the arm starts
     (see handleDebug.ino - 'z' zeroes min_max_block/histogram/counters; min_free_heap is
     the native allocator watermark and is NOT resettable by design).
  2. scripts/capture-mqtt-debug.bat running for the arm's duration, producing ONE merged
     transcript (telnet + MQTT + browser + crashlog + curl-probes incl. /api/v2/device/info -
     TASK-1017) per arm.
  3. A concurrent safety watchdog polling telnet reachability, heap floor (via
     /api/v2/device/info's hd_min_free_heap field - TASK-1017; falls back to the telnet
     banner's "minFree" field if the JSON endpoint is unreachable), and the crashlog
     endpoint. On ANY incident it aborts the
     capture process early and writes an incident record next to the transcript - the sweep
     driver (TASK-1022) can then skip to the next arm instead of hanging on a wedged device.

This script does not itself vary the gate-cap build flags (TASK-1017 handles that at build
time); it is invoked once per already-flashed arm.

USAGE
-----
  python scripts/loadtest_arm_runner.py --host 192.168.88.64 --arm-name N4 --duration 60 \
      --broker-host homeassistant.local --output-root logs/loadtest-sweep
"""

import argparse
import json
import os
import re
import shutil
import socket
import subprocess
import sys
import threading
import time
import urllib.request

DEFAULT_HEAP_FLOOR_BYTES = 20000  # below this, treat as an incident (device near-OOM)


def kill_process_tree(proc):
    """proc.terminate()/kill() on Windows only signals the top-level cmd.exe wrapper that
    ran the .bat, not the PowerShell child it spawns internally - the capture keeps running
    and communicate() hangs until its own timeout. taskkill /T (tree) /F (force) is required
    to actually stop a launched .bat's descendants."""
    if os.name == "nt":
        subprocess.run(["taskkill", "/PID", str(proc.pid), "/T", "/F"],
                        capture_output=True, check=False)
    else:
        proc.terminate()


def telnet_reachable(host, timeout=3.0):
    try:
        with socket.create_connection((host, 23), timeout=timeout):
            return True
    except OSError:
        return False


def fetch_json(url, timeout=3.0):
    try:
        with urllib.request.urlopen(url, timeout=timeout) as r:
            return json.loads(r.read().decode())
    except Exception:
        return None


def fetch_telnet_banner_min_free(host, timeout=3.0):
    """Fallback heap read via the telnet welcome banner's 'minFree <N>' field."""
    try:
        with socket.create_connection((host, 23), timeout=timeout) as s:
            s.settimeout(timeout)
            data = s.recv(4096).decode(errors="replace")
    except OSError:
        return None
    m = re.search(r"minFree\s+(\d+)", data)
    return int(m.group(1)) if m else None


class Watchdog:
    """Polls telnet/stats/crashlog every --poll-interval seconds; aborts the arm on incident."""

    def __init__(self, host, heap_floor, poll_interval, on_incident):
        self.host = host
        self.heap_floor = heap_floor
        self.poll_interval = poll_interval
        self.on_incident = on_incident
        self._stop = threading.Event()
        self._thread = None
        self.incidents = []

    def _record(self, kind, detail):
        incident = {"ts": time.time(), "kind": kind, "detail": detail}
        self.incidents.append(incident)
        self.on_incident(incident)

    def _poll_once(self):
        if not telnet_reachable(self.host):
            self._record("telnet_unreachable", f"host={self.host}")
            return

        # TASK-1017 landed its hwm/counter stats on the existing /api/v2/device/info
        # payload (hd_* fields) - there is no dedicated /api/v2/device/info route.
        info = fetch_json(f"http://{self.host}/api/v2/device/info")
        min_free = None
        if info:
            min_free = info.get("hd_min_free_heap")
        if min_free is None:
            min_free = fetch_telnet_banner_min_free(self.host)
        if min_free is not None and min_free < self.heap_floor:
            self._record("heap_below_floor", f"min_free={min_free} floor={self.heap_floor}")
            return

        crashlog = fetch_json(f"http://{self.host}/api/v2/device/crashlog")
        if crashlog and crashlog.get("reason") not in (None, "", "none", "None"):
            self._record("crashlog_present", json.dumps(crashlog))

    def _loop(self):
        while not self._stop.wait(self.poll_interval):
            try:
                self._poll_once()
            except Exception as exc:
                self._record("watchdog_error", str(exc))
            if self.incidents:
                break

    def start(self):
        self._thread = threading.Thread(target=self._loop, daemon=True)
        self._thread.start()

    def stop(self):
        self._stop.set()
        if self._thread:
            self._thread.join(timeout=5)


def telnet_reset_diagnostics(host, timeout=5.0):
    """Send 'z' to zero the heap-soak hwm/histogram/counters before the arm starts."""
    try:
        with socket.create_connection((host, 23), timeout=timeout) as s:
            s.settimeout(timeout)
            try:
                s.recv(4096)
            except OSError:
                pass
            s.sendall(b"z")
            time.sleep(0.3)
        return True
    except OSError:
        return False


def find_latest_transcript(output_dir):
    """capture-mqtt-debug.bat creates its own timestamped subfolder under -OutputRoot,
    so this must walk, not just list the top level."""
    if not os.path.isdir(output_dir):
        return None
    candidates = []
    for root, _dirs, files in os.walk(output_dir):
        for f in files:
            if f.startswith("transcript-") and f.endswith(".txt"):
                candidates.append(os.path.join(root, f))
    if not candidates:
        return None
    return max(candidates, key=os.path.getmtime)


def run_arm(args):
    arm_output_root = os.path.join(args.output_root, args.arm_name)
    os.makedirs(arm_output_root, exist_ok=True)

    if not telnet_reset_diagnostics(args.host):
        print(f"WARNING: could not reach telnet on {args.host} to reset diagnostics before the arm", file=sys.stderr)

    incident_holder = {}

    capture_proc = None

    def on_incident(incident):
        incident_holder["incident"] = incident
        print(f"WATCHDOG INCIDENT: {incident}", file=sys.stderr)
        if capture_proc and capture_proc.poll() is None:
            kill_process_tree(capture_proc)

    watchdog = Watchdog(args.host, args.heap_floor, args.poll_interval, on_incident)
    watchdog.start()

    capture_script = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "scripts", "capture-mqtt-debug.bat")
    cmd = [
        capture_script,
        "-DeviceHost", args.host,
        "-BrokerHost", args.broker_host,
        "-DurationSeconds", str(int(args.duration)),
        "-OutputRoot", arm_output_root,
        "-SkipBrowserCapture",
    ]

    t0 = time.monotonic()
    capture_proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    try:
        stdout, stderr = capture_proc.communicate(timeout=args.duration + 60)
    except subprocess.TimeoutExpired:
        kill_process_tree(capture_proc)
        try:
            stdout, stderr = capture_proc.communicate(timeout=10)
        except subprocess.TimeoutExpired:
            stdout, stderr = "", "capture process tree did not exit after taskkill /T /F"
    wall_s = time.monotonic() - t0

    watchdog.stop()

    transcript = find_latest_transcript(arm_output_root)
    result = {
        "arm_name": args.arm_name,
        "host": args.host,
        "wall_seconds": round(wall_s, 1),
        "transcript_path": transcript,
        "aborted": "incident" in incident_holder,
        "incidents": watchdog.incidents,
        "capture_returncode": capture_proc.returncode,
    }

    if "incident" in incident_holder:
        incident_path = os.path.join(arm_output_root, "incident.json")
        with open(incident_path, "w", encoding="utf-8") as f:
            json.dump(incident_holder["incident"], f, indent=2)
        result["incident_path"] = incident_path

    return result


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--host", required=True, help="Device IP")
    p.add_argument("--arm-name", required=True, help="Arm label, e.g. N4 (used as the transcript subdirectory)")
    p.add_argument("--duration", type=float, default=60.0, help="Arm duration in seconds")
    p.add_argument("--broker-host", default="homeassistant.local", help="MQTT broker host for capture-mqtt-debug.bat (non-interactive - required even if unreachable)")
    p.add_argument("--output-root", default="logs/loadtest-sweep", help="Root directory; one subdir per arm is created under it")
    p.add_argument("--heap-floor", type=int, default=DEFAULT_HEAP_FLOOR_BYTES, help="Abort the arm if min_free_heap drops below this many bytes")
    p.add_argument("--poll-interval", type=float, default=5.0, help="Watchdog poll interval in seconds")
    args = p.parse_args()

    if shutil.which("pwsh") is None and shutil.which("powershell") is None:
        print("WARNING: neither pwsh nor powershell found on PATH - capture-mqtt-debug.bat will fail to launch", file=sys.stderr)

    result = run_arm(args)
    print(json.dumps(result, indent=2, default=str))
    sys.exit(1 if result["aborted"] else 0)


if __name__ == "__main__":
    main()
