"""
loadtest_combined.py - "S-combined" worst-case orchestrator for TASK-1015
============================================================================

PURPOSE
-------
Runs all four load sources from the TASK-1015 study SIMULTANEOUSLY for a fixed
duration, each independently measurable, so the parallelism-optimum sweep (TASK-1022)
can be run once under "real-world worst case" conditions instead of one source at a time:

  1. Browser page-loads  - repeated cold+warm navigations (scripts/loadtest_pageload.py)
  2. API polling          - concurrent REST+asset requests (scripts/loadtest_harness.py)
  3. WS live-log subscriber - a client that stays connected to /ws and counts frames
  4. OT bus traffic       - see OT-SOURCE SELECTION below

OT-SOURCE SELECTION
--------------------
The task description names sat_boiler_emulator.py, but that script only works against
an OTDirect TCP bridge (port 25238) - i.e. OTGW32/OT-Thing boards (HAS_DIRECT_OT).
This harness auto-detects the target board's OT path and picks accordingly:

  - OTDirect board (port 25238 reachable): drives scripts/sat_boiler_emulator.py
  - PIC board (e.g. esp32-classic bench unit, no OTDirect bridge): toggles the
    firmware's own on-device "OTGW serial-simulation replay" (telnet debug key 's',
    state.debug.bOTGWSimulation - see handleDebug.ino:439-449) for the run duration.
    This is the only OT-traffic generator available on a PIC-path board without a
    live boiler wired to the bus, and it is a real firmware feature, not a bolted-on
    substitute.

Each source's metrics are collected independently and printed as one JSON summary.

USAGE
-----
  python scripts/loadtest_combined.py --host 192.168.88.64 --duration 30 --api-concurrency 4
"""

import argparse
import json
import socket
import subprocess
import sys
import threading
import time

try:
    import websocket  # websocket-client
except ImportError:
    print("Missing dependency: pip install websocket-client", file=sys.stderr)
    sys.exit(1)


def tcp_port_open(host, port, timeout=1.5):
    try:
        with socket.create_connection((host, port), timeout=timeout):
            return True
    except OSError:
        return False


def telnet_toggle_otgw_sim(host, enable, timeout=5.0):
    """Send the 's' debug key to flip state.debug.bOTGWSimulation, return the banner line."""
    key = b"s"
    with socket.create_connection((host, 23), timeout=timeout) as s:
        s.settimeout(timeout)
        try:
            s.recv(4096)  # discard welcome banner
        except OSError:
            pass
        s.sendall(key)
        time.sleep(0.5)
        try:
            reply = s.recv(4096).decode(errors="replace")
        except OSError:
            reply = ""
    return reply


class WsSubscriber:
    """Stays connected to /ws for the run duration, counts frames and errors."""

    def __init__(self, host):
        self.url = f"ws://{host}/ws"
        self.frame_count = 0
        self.error_count = 0
        self.connected = False
        self._stop = threading.Event()
        self._thread = None

    def _loop(self):
        try:
            ws = websocket.create_connection(self.url, timeout=10)
            self.connected = True
        except Exception:
            self.error_count += 1
            return
        ws.settimeout(1.0)
        while not self._stop.is_set():
            try:
                ws.recv()
                self.frame_count += 1
            except websocket.WebSocketTimeoutException:
                continue
            except Exception:
                self.error_count += 1
                break
        try:
            ws.close()
        except Exception:
            pass

    def start(self):
        self._thread = threading.Thread(target=self._loop, daemon=True)
        self._thread.start()

    def stop(self):
        self._stop.set()
        if self._thread:
            self._thread.join(timeout=5)

    def summary(self):
        return {"connected": self.connected, "frame_count": self.frame_count, "error_count": self.error_count}


def run_browser_loop(host, duration, results):
    """Repeated cold page-loads for the run duration (imports loadtest_pageload directly)."""
    import loadtest_pageload as plp
    import os
    import shutil
    import tempfile

    edge_path = plp.find_edge(None)
    url = f"http://{host}/v2.html"
    runs = []
    deadline = time.monotonic() + duration
    tmp_root = tempfile.mkdtemp(prefix="otgw_combined_browser_")
    try:
        i = 0
        while time.monotonic() < deadline:
            profile = tempfile_dir = f"{tmp_root}/p{i}"
            os.makedirs(profile, exist_ok=True)
            try:
                runs.append(plp.run_one(edge_path, url, profile, f"combined-{i}"))
            except SystemExit as exc:
                runs.append({"error": str(exc)})
                break
            i += 1
    finally:
        shutil.rmtree(tmp_root, ignore_errors=True)
    results["browser"] = {"navigations": len(runs), "runs": runs}


def run_api_load(host, duration, concurrency, results):
    proc = subprocess.run(
        [sys.executable, "scripts/loadtest_harness.py",
         "--host", host, "--concurrency", str(concurrency), "--duration", str(duration)],
        capture_output=True, text=True,
    )
    results["api"] = {"returncode": proc.returncode, "stdout_tail": proc.stdout[-2000:], "stderr_tail": proc.stderr[-500:]}


def run_ws_subscriber(host, duration, results):
    sub = WsSubscriber(host)
    sub.start()
    time.sleep(duration)
    sub.stop()
    results["ws"] = sub.summary()


def run_ot_source(host, duration, results):
    if tcp_port_open(host, 25238):
        # sat_boiler_emulator.py has no --duration flag (it runs until Ctrl-C / SIGTERM
        # by design), so bound it externally with a process timeout instead.
        proc = subprocess.Popen(
            [sys.executable, "scripts/sat_boiler_emulator.py", "--host", host],
            stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True,
        )
        try:
            stdout, stderr = proc.communicate(timeout=duration)
        except subprocess.TimeoutExpired:
            proc.terminate()
            stdout, stderr = proc.communicate(timeout=5)
        results["ot"] = {"source": "otdirect_emulator", "returncode": proc.returncode, "stdout_tail": stdout[-1000:]}
        return
    # handleDebug.ino's 's' handler prints "Debug OTGW serial simulation: <true|false>"
    # via CBOOLEAN(state.debug.bOTGWSimulation) - match that literal, not English prose.
    on_banner = telnet_toggle_otgw_sim(host, enable=True)
    time.sleep(duration)
    off_banner = telnet_toggle_otgw_sim(host, enable=False)
    results["ot"] = {
        "source": "onboard_serial_simulation_replay",
        "enabled_confirmed": "simulation: true" in on_banner.lower(),
        "disabled_confirmed": "simulation: false" in off_banner.lower(),
    }


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--host", required=True, help="Device IP, e.g. 192.168.88.64")
    p.add_argument("--duration", type=float, default=30.0, help="Combined run duration in seconds")
    p.add_argument("--api-concurrency", type=int, default=4, help="Offered concurrency for the API load source")
    args = p.parse_args()

    results = {}
    threads = [
        threading.Thread(target=run_browser_loop, args=(args.host, args.duration, results)),
        threading.Thread(target=run_api_load, args=(args.host, args.duration, args.api_concurrency, results)),
        threading.Thread(target=run_ws_subscriber, args=(args.host, args.duration, results)),
        threading.Thread(target=run_ot_source, args=(args.host, args.duration, results)),
    ]
    t0 = time.monotonic()
    for t in threads:
        t.start()
    for t in threads:
        t.join()
    wall_s = time.monotonic() - t0

    print(json.dumps({"wall_seconds": round(wall_s, 1), **results}, indent=2, default=str))


if __name__ == "__main__":
    main()
