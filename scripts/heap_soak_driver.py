"""
heap_soak_driver.py - representative-load heap soak (TASK-956, TASK-1036)
=========================================================================

PURPOSE
-------
Drives a REPRESENTATIVE (not adversarial) load profile against one dedicated
ESP32-S3 for hours, and records the TASK-934 heap-diagnostics counters from
GET /api/v2/device/info: hd_min_max_block, hd_min_free_heap,
hd_max_loop_gap_ms, hd_enter_low/warning/critical, hd_ws_drops,
hd_mqtt_drops, hd_rest_503 and hd_webfile_503. TASK-956 used it to decide
whether the preventive heap-frag gating was still needed. TASK-1036 re-soaks
with it after that gating was removed (ADR-167).

This is not an overload ramp; that is scripts/loadtest_harness.py and
loadtest_sweep.py. The profile mimics normal use: occasional page loads,
periodic API polling and background OT traffic, sustained for hours.

WHAT IT DOES
  1. GET /api/v2/device/info must answer, or the run does not start.
  2. Telnet 'z' zeroes the heap-diag window (resetHeapWatermark). Start the
     driver from a healthy heap. Port 23 serves one client
     (AsyncSimpleTelnet<1>), so start any passive telnet reader after this.
  3. POST /api/v2/simulate/start switches on the onboard OT frame replay.
     Since TASK-1071 the replay runs on PIC and OT-Direct boards alike. The
     call is idempotent. The telnet 's' key it replaces is a toggle, so after
     a mid-run reboot it would switch the replay back on at the end.
  4. Every --poll-interval-sec: one load request, then one snapshot. A
     snapshot is GET /api/v2/device/info plus GET /api/v2/simulate. Requests
     go out one at a time (ADR-165 allows at most 2 in flight).
  5. With --republish-every-min N: every N minutes, POST
     /api/v2/discovery/republish, log the HTTP status, and track whether
     disc_pending_ids drains to 0. The first POST goes out after N minutes.
     No POST goes out when fewer than N minutes remain, so every POST gets a
     full drain window. The firmware answers 429 to a POST within 60 s of
     the last successful one, and 503 while MQTT is not connected.
  6. At the end: a final snapshot, POST /api/v2/simulate/stop, and one
     SUMMARY line on stdout. Progress and anomalies go to stderr. Every
     record goes to --snapshot-log (ndjson).

BEFORE A SOAK
  - /otgw_simulation.log must be on the device's LittleFS. It is not part of
    the filesystem image: upload it the way scripts/tests/run_coverage_test.py
    does. A filesystem flash removes it, and without it the replay switches
    itself off, which shows here as sim_inactive.
  - Turn the nightly restart off (settings: nightly restart). A run across its
    hour otherwise ends with a correct but unwanted 'reboot' anomaly.

The ndjson phases are replay_started, baseline, running, final, poll_failed,
anomaly, republish, republish_drained, replay_stopped (or replay_stop_failed)
and summary, or only not_started. The TASK-956 logs used "unreachable" for a
failed poll; here that is poll_failed.

ANOMALIES (any one makes the exit code 1)
  reboot            bootcount, lastreset or fwversion changed, or uptime went
                    down, between two snapshots. bootcount alone is not
                    enough: updateRebootCount() returns N+1 even when it
                    cannot write /reboot_count.txt, so two boots can report
                    the same count.
  unreachable       no complete snapshot for longer than
                    --max-unreachable-sec. The record carries the last error:
                    "no response", an HTTP error such as "HTTP 503 low heap"
                    (device/info answers 503 while the largest free block is
                    below 8192 B), or none when no request failed in that
                    window (the driver was not polling, or answers were slow).
  sim_inactive      GET /api/v2/simulate reported active=false. A reboot
                    clears the replay flag. The driver does not switch it
                    back on.
  republish_failed  a republish POST did not answer 200 marked_pending.
  republish_not_drained
                    disc_pending_ids did not reach 0 before the next POST or
                    the end of the run.
  interrupted       the run was stopped with Ctrl-C.

The heap numbers in the summary are reported, not judged: the maxima of the
counters and the minima of hd_min_max_block and hd_min_free_heap. Judge them
against the task's pass criteria. A failed telnet 'z' shows as
z_reset=failed. It can only make those numbers look worse, so it is not an
anomaly.

disc_republish_triggered is not used. It counts only the republishes that the
discovery verify pass triggers (mqtt_discovery_verify.cpp). A REST republish
calls markAllMQTTConfigPending() directly and never increments it.

The POSTs carry no credentials. With an admin password set on the device,
POST /api/v2/simulate/start answers 401 and the run does not start.

Exit codes: 0 clean run, 1 anomaly, 2 the run could not start.

USAGE
  python scripts/heap_soak_driver.py --host <device-ip> --duration-hours 10
  python scripts/heap_soak_driver.py --host <device-ip> --duration-hours 10 --republish-every-min 30 --snapshot-log build/soak.ndjson
"""

import argparse
import json
import os
import random
import re
import socket
import sys
import time
import urllib.error
import urllib.request

ASSET_PATHS = ["/v2.html", "/v2.css", "/ds-tokens.css", "/v2.js"]
API_PATHS = ["/api/v2/device/info", "/api/v2/sat/status", "/api/v2/health"]
HTTP_TIMEOUT_SEC = 5.0
# Pause before every JSON request. The REST gate frees a slot when the previous
# connection's disconnect callback runs, which can trail the response a little;
# at REST cap 1 (largest free block < 16000 B) a request sent at once could get
# a self-inflicted 503 "Server busy" and inflate hd_rest_503, a soak metric.
REQUEST_SPACING_SEC = 0.3
FIRMWARE_REPUBLISH_COOLDOWN_SEC = 60  # REPUBLISH_COOLDOWN_MS in restAPI.ino

# device/info keys copied into every snapshot record.
SNAPSHOT_KEYS = [
    "fwversion", "bootcount", "lastreset", "uptime", "otgwsimulation",
    "freeheap", "maxfreeblock", "hd_min_max_block", "hd_min_free_heap",
    "hd_max_loop_gap_ms", "hd_enter_low", "hd_enter_warning", "hd_enter_critical",
    "hd_drip_slowmode", "hd_ws_drops", "hd_mqtt_drops", "hd_rest_503",
    "hd_webfile_503", "disc_pending_ids",
]
# Reported in the summary as their maximum (counters) or minimum (watermarks).
MAX_KEYS = ["hd_enter_low", "hd_enter_warning", "hd_enter_critical",
            "hd_ws_drops", "hd_mqtt_drops", "hd_max_loop_gap_ms",
            "hd_drip_slowmode", "hd_rest_503", "hd_webfile_503"]
MIN_KEYS = ["hd_min_max_block", "hd_min_free_heap"]
# A change in any of these between two snapshots means the device restarted.
IDENTITY_KEYS = ["bootcount", "lastreset", "fwversion"]

# upTime() in helperStuff.ino prints "%d(d)-%02d:%02d(H:m)": days, hours, minutes.
UPTIME_RE = re.compile(r"^(\d+)\(d\)-(\d+):(\d+)\(H:m\)$")


def uptime_minutes(text):
    """Minutes in the firmware uptime string, or None when it does not parse."""
    m = UPTIME_RE.match(text) if isinstance(text, str) else None
    if not m:
        return None
    days, hours, minutes = (int(g) for g in m.groups())
    return (days * 24 + hours) * 60 + minutes


def telnet_send(host, port, key, timeout=5.0):
    try:
        with socket.create_connection((host, port), timeout=timeout) as s:
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


def fetch_status(base, path, timeout=HTTP_TIMEOUT_SEC):
    """Load request. Reads the status only, not the body, as the TASK-956 runs
    did. Keep it that way so the load stays comparable with that baseline."""
    try:
        with urllib.request.urlopen(base + path, timeout=timeout) as r:
            return r.status
    except Exception:
        return None


def describe_http_error(status, body):
    """'HTTP 503 low heap' from the firmware's {"error":{"message":...}} body."""
    message = ""
    if isinstance(body, dict):
        err = body.get("error")
        if isinstance(err, dict):
            message = str(err.get("message", ""))
        elif body.get("detail"):
            message = str(body["detail"])
    return f"HTTP {status} {message}".strip()


def http_request(base, method, path, timeout=HTTP_TIMEOUT_SEC):
    """One request. Returns (status, json_body, error). status is None when no
    HTTP response arrived. json_body is None when the body is not JSON. error
    is None for a 2xx answer."""
    time.sleep(REQUEST_SPACING_SEC)
    req = urllib.request.Request(base + path, method=method,
                                 data=b"" if method == "POST" else None)
    try:
        with urllib.request.urlopen(req, timeout=timeout) as r:
            status, raw = r.status, r.read()
    except urllib.error.HTTPError as e:
        status = e.code
        try:
            raw = e.read()
        except Exception:
            raw = b""
    except Exception as e:  # URLError, timeout, reset, closed without response
        return None, None, f"no response ({type(e).__name__}: {e})"
    try:
        body = json.loads(raw.decode("utf-8", "replace"))
    except ValueError:
        body = None
    if 200 <= status < 300:
        return status, body, None
    return status, body, describe_http_error(status, body)


def summary_value(value):
    if value is None:
        return "null"
    if isinstance(value, str):
        return json.dumps(value) if (" " in value or not value) else value
    return str(value)


class SoakRun:
    def __init__(self, args, log_fh):
        self.args = args
        netloc = args.host if args.port == 80 else f"{args.host}:{args.port}"
        self.base = f"http://{netloc}"
        self.log_fh = log_fh
        self.anomalies = []          # anomaly kinds, in the order they were flagged
        self.first = None            # first complete snapshot
        self.prev = None             # latest complete snapshot
        self.last_ok = None          # monotonic time of the latest complete snapshot
        self.gap_flagged = False
        self.last_error = None
        self.longest_gap_s = 0.0
        self.snapshots_ok = 0
        self.snapshots_failed = 0
        self.devinfo_503 = 0
        self.sim_inactive = 0
        self.sim_flagged = False
        self.maxima = {}
        self.minima = {}
        self.requests = 0            # load requests
        self.z_reset = None
        self.deadline = None
        self.soak_start = None
        self.interval_s = args.republish_every_min * 60
        self.next_republish = float("inf")
        self.drain_since = None      # monotonic time of a 200 that has not drained yet
        self.rp = {"sent": 0, "200": 0, "429": 0, "503": 0, "other": 0,
                   "drained": 0, "max_drain_s": None}

    # --- output ---------------------------------------------------------
    def log(self, phase, **fields):
        self.log_fh.write(json.dumps({"ts": time.time(), "phase": phase, **fields}) + "\n")
        self.log_fh.flush()

    def flag(self, kind, **detail):
        self.anomalies.append(kind)
        self.log("anomaly", kind=kind, **detail)
        print(f"ANOMALY {kind}: {json.dumps(detail)}", file=sys.stderr)

    def progress(self, snap):
        left_min = max(0.0, (self.deadline - time.monotonic()) / 60)
        print(f"[{left_min:.1f} min left] requests={self.requests} "
              f"freeheap={snap['freeheap']} maxblock={snap['maxfreeblock']} "
              f"hd_min_max_block={snap['hd_min_max_block']} "
              f"enter_low/warn/crit={snap['hd_enter_low']}/{snap['hd_enter_warning']}/"
              f"{snap['hd_enter_critical']} drops_ws/mqtt={snap['hd_ws_drops']}/"
              f"{snap['hd_mqtt_drops']} sim={'on' if snap['sim_active'] else 'OFF'} "
              f"bootcount={snap['bootcount']} uptime={snap['uptime']} "
              f"pending_ids={snap['disc_pending_ids']}", file=sys.stderr)

    # --- snapshots ------------------------------------------------------
    def snapshot(self):
        """(snapshot, None) or (None, error). A snapshot needs both GETs."""
        status, body, err = http_request(self.base, "GET", "/api/v2/device/info")
        device = body.get("device") if isinstance(body, dict) else None
        if status != 200 or not isinstance(device, dict):
            if status == 503:
                self.devinfo_503 += 1
            return None, "device/info: " + (err or f"HTTP {status} without a device object")
        snap = {k: device.get(k) for k in SNAPSHOT_KEYS}
        snap["uptime_min"] = uptime_minutes(snap["uptime"])
        status, body, err = http_request(self.base, "GET", "/api/v2/simulate")
        sim = body.get("simulation") if isinstance(body, dict) else None
        if status != 200 or not isinstance(sim, dict):
            return None, "simulate: " + (err or f"HTTP {status} without a simulation object")
        snap["sim_active"] = sim.get("active") is True
        return snap, None

    def poll(self, phase):
        snap, err = self.snapshot()
        now = time.monotonic()
        gap = now - self.last_ok
        self.longest_gap_s = max(self.longest_gap_s, gap)
        if snap is None:
            self.snapshots_failed += 1
            self.last_error = err
            self.log("poll_failed", error=err, since_last_snapshot_s=round(gap, 1))
            print(f"WARNING: snapshot failed: {err}", file=sys.stderr)
            self.check_gap(gap)
            return
        self.check_gap(gap)
        self.last_ok, self.gap_flagged, self.last_error = now, False, None
        self.snapshots_ok += 1
        self.log(phase, requests_so_far=self.requests, **snap)
        self.check_snapshot(snap, now)
        self.progress(snap)

    def check_gap(self, gap):
        if gap > self.args.max_unreachable_sec and not self.gap_flagged:
            self.gap_flagged = True
            self.flag("unreachable", no_snapshot_for_s=round(gap, 1), last_error=self.last_error)

    def check_snapshot(self, snap, now):
        if self.first is None:
            self.first = snap
        if self.prev is not None:
            signals = [f"{k} {self.prev[k]!r} -> {snap[k]!r}"
                       for k in IDENTITY_KEYS if snap[k] != self.prev[k]]
            before, after = self.prev["uptime_min"], snap["uptime_min"]
            if before is not None and after is not None and after < before:
                signals.append(f"uptime {self.prev['uptime']} -> {snap['uptime']}")
            if signals:
                self.flag("reboot", signals=signals)
        self.prev = snap

        if snap["sim_active"]:
            self.sim_flagged = False
        else:
            self.sim_inactive += 1
            if not self.sim_flagged:
                self.sim_flagged = True
                self.flag("sim_inactive", otgwsimulation=snap["otgwsimulation"])

        for k in MAX_KEYS:
            v = snap[k]
            if isinstance(v, (int, float)) and not isinstance(v, bool):
                self.maxima[k] = max(v, self.maxima.get(k, v))
        for k in MIN_KEYS:
            v = snap[k]
            if isinstance(v, (int, float)) and not isinstance(v, bool):
                self.minima[k] = min(v, self.minima.get(k, v))

        if self.drain_since is not None and snap["disc_pending_ids"] == 0:
            drain_s = round(now - self.drain_since, 1)
            self.drain_since = None
            self.rp["drained"] += 1
            self.rp["max_drain_s"] = max(drain_s, self.rp["max_drain_s"] or 0.0)
            self.log("republish_drained", after_s=drain_s)

    # --- load and republish ---------------------------------------------
    def load_request(self):
        # Mostly API polls (dashboard refresh), sometimes a full page reload.
        if random.random() < 0.15:
            for path in ASSET_PATHS:
                fetch_status(self.base, path)
                self.requests += 1
                time.sleep(0.3)  # sequential, like a single-flight client
        else:
            fetch_status(self.base, random.choice(API_PATHS))
            self.requests += 1

    def flag_not_drained(self, now):
        self.flag("republish_not_drained",
                  pending_ids=self.prev["disc_pending_ids"] if self.prev else None,
                  waited_s=round(now - self.drain_since, 1))
        self.drain_since = None

    def maybe_republish(self):
        now = time.monotonic()
        if now < self.next_republish:
            return
        if self.deadline - now < self.interval_s:
            self.next_republish = float("inf")  # no full drain window left
            return
        if self.drain_since is not None:
            self.flag_not_drained(now)
        status, body, err = http_request(self.base, "POST", "/api/v2/discovery/republish")
        self.next_republish = time.monotonic() + self.interval_s
        self.rp["sent"] += 1
        count = body.get("count") if isinstance(body, dict) else None
        self.log("republish", status=status, count=count, error=err)
        print(f"republish: HTTP {status} count={count}" + (f" ({err})" if err else ""),
              file=sys.stderr)
        if status == 200 and isinstance(body, dict) and body.get("status") == "marked_pending":
            self.rp["200"] += 1
            self.drain_since = time.monotonic()
        else:
            self.rp[str(status) if status in (429, 503) else "other"] += 1
            self.flag("republish_failed", status=status,
                      error=err or "answered without status marked_pending")

    # --- run --------------------------------------------------------------
    def not_started(self, reason):
        print(f"ERROR: {reason}", file=sys.stderr)
        self.log("not_started", reason=reason)
        print("SUMMARY verdict=NOT_STARTED reason=" + json.dumps(reason))
        return 2

    def stop_replay(self):
        status, body, err = http_request(self.base, "POST", "/api/v2/simulate/stop")
        sim = body.get("simulation") if isinstance(body, dict) else None
        stopped = status == 200 and isinstance(sim, dict) and sim.get("active") is False
        self.log("replay_stopped" if stopped else "replay_stop_failed",
                 status=status, simulation=sim, error=err)
        if not stopped:
            print(f"WARNING: POST /api/v2/simulate/stop failed ({err or status}); "
                  "the replay may still be running", file=sys.stderr)

    def run(self):
        a = self.args
        status, body, err = http_request(self.base, "GET", "/api/v2/device/info")
        if status != 200 or not isinstance(body, dict) or not isinstance(body.get("device"), dict):
            return self.not_started("GET /api/v2/device/info failed: " + (err or f"HTTP {status}"))

        print("Resetting the heap-diag window (telnet 'z')...", file=sys.stderr)
        self.z_reset = telnet_send(a.host, a.telnet_port, "z")
        if not self.z_reset:
            print("WARNING: telnet 'z' failed; the counters include history from before "
                  "this run", file=sys.stderr)

        print("Starting the OT frame replay (POST /api/v2/simulate/start)...", file=sys.stderr)
        status, body, err = http_request(self.base, "POST", "/api/v2/simulate/start")
        sim = body.get("simulation") if isinstance(body, dict) else None
        if status != 200 or not isinstance(sim, dict) or sim.get("active") is not True:
            hint = {401: " (an admin password is set; this driver sends no credentials)",
                    409: " (LittleFS is not mounted, so the replay fixture cannot be read)"}
            if status == 200:
                http_request(self.base, "POST", "/api/v2/simulate/stop")
            return self.not_started("POST /api/v2/simulate/start did not start the replay: "
                                    + (err or f"HTTP {status}, simulation={sim}")
                                    + hint.get(status, ""))
        self.log("replay_started", simulation=sim)

        self.soak_start = self.last_ok = time.monotonic()
        self.deadline = self.soak_start + a.duration_hours * 3600
        if self.interval_s > 0:
            self.next_republish = self.soak_start + self.interval_s
        try:
            self.poll("baseline")
            while time.monotonic() < self.deadline:
                self.load_request()
                time.sleep(a.poll_interval_sec)
                self.poll("running")
                self.maybe_republish()
            self.poll("final")
        except KeyboardInterrupt:
            print("Interrupted by user.", file=sys.stderr)
            self.flag("interrupted")
        finally:
            self.stop_replay()
        return self.finish()

    def finish(self):
        if self.drain_since is not None:
            self.flag_not_drained(time.monotonic())
        counts = {}
        for kind in self.anomalies:
            counts[kind] = counts.get(kind, 0) + 1
        s = {
            "verdict": "ANOMALY" if self.anomalies else "CLEAN",
            "anomalies": ",".join(f"{k}:{v}" for k, v in counts.items()) or "none",
            "duration_h": round((time.monotonic() - self.soak_start) / 3600, 2),
            "snapshots_ok": self.snapshots_ok,
            "snapshots_failed": self.snapshots_failed,
            "longest_gap_s": round(self.longest_gap_s, 1),
            "devinfo_503": self.devinfo_503,
            "load_requests": self.requests,
            "z_reset": "ok" if self.z_reset else "failed",
        }
        for k in ("fwversion", "bootcount", "lastreset", "uptime"):
            at_start = self.first.get(k) if self.first else None
            at_end = self.prev.get(k) if self.prev else None
            s[k] = at_start if at_start == at_end else f"{at_start} -> {at_end}"
        s["sim_inactive_snapshots"] = self.sim_inactive
        for k in MAX_KEYS:
            s[k + "_max"] = self.maxima.get(k)
        for k in MIN_KEYS:
            s[k + "_min"] = self.minima.get(k)
        if self.interval_s > 0:
            for k, v in self.rp.items():
                s["republish_" + k] = v
        self.log("summary", **s)
        print("SUMMARY " + " ".join(f"{k}={summary_value(v)}" for k, v in s.items()))
        return 1 if self.anomalies else 0


def parse_args(argv):
    p = argparse.ArgumentParser(description=__doc__,
                                formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--host", required=True)
    p.add_argument("--port", type=int, default=80, help="HTTP port")
    p.add_argument("--telnet-port", type=int, default=23,
                   help="debug telnet port, used for the 'z' reset")
    p.add_argument("--duration-hours", type=float, default=2.0)
    p.add_argument("--poll-interval-sec", type=float, default=20.0,
                   help="cadence of one load request plus one snapshot")
    p.add_argument("--max-unreachable-sec", type=float, default=120.0,
                   help="flag 'unreachable' after this long without a complete snapshot; "
                        "keep it well above the poll interval")
    p.add_argument("--republish-every-min", type=float, default=0.0,
                   help="POST /api/v2/discovery/republish every N minutes (0 = off); "
                        "needs MQTT connected")
    p.add_argument("--snapshot-log", default="build/heap_soak_snapshots.ndjson")
    args = p.parse_args(argv)
    if args.duration_hours <= 0 or args.poll_interval_sec <= 0 or args.republish_every_min < 0:
        p.error("--duration-hours and --poll-interval-sec must be positive, "
                "--republish-every-min must not be negative")
    if args.max_unreachable_sec <= args.poll_interval_sec:
        p.error("--max-unreachable-sec must be larger than --poll-interval-sec")
    return args


def main(argv=None):
    args = parse_args(argv)
    if 0 < args.republish_every_min * 60 < FIRMWARE_REPUBLISH_COOLDOWN_SEC:
        print("WARNING: the firmware answers 429 to a republish within 60 s of the last one",
              file=sys.stderr)
    os.makedirs(os.path.dirname(os.path.abspath(args.snapshot_log)), exist_ok=True)
    with open(args.snapshot_log, "w", encoding="utf-8") as log_fh:
        return SoakRun(args, log_fh).run()


if __name__ == "__main__":
    sys.exit(main())
