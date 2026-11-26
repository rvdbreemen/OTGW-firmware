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

REST IN-FLIGHT CAP SWEEP (TASK-3091, --restcap)
-----------------------------------------------
The Phase 1 sweep above varies the OFFERED concurrency against a fixed gate. --restcap does the
opposite: the offered load is fixed (--offered, default 8 clients) and the GATE is set per arm through
the prerelease runtime override (POST /api/v2/debug/flags {"name":"restcap","value":"N"}, read back
before every repeat). Per arm N (default 2..8) at offered 8 (judged) and offered 4 (realistic, reported), --repeats
(default 3) rounds of --seconds (default 60). The pooled N=2 arm at offered 8 runs between all other arms
(2,4,2,6,2,8,...), the other arms rotate each round, so slow drift is not credited to an N; plus a no-load
control arm (--no-control to skip). The rule is the one confirmed by the architect (RULE). Per repeat it records: client-visible 503 and other failures,
p50/p95/p99 latency, the device's own counters (bootcount, hd_min_free_heap, hd_min_max_block,
hd_rest_503, hd_rest_inflight_hwm, hd_ws_disconnects), new crashlogs, a 1 Hz sample of freeheap and maxfreeblock,
the EFFECTIVE cap (min(override, heap tier): maxblk < 24000 gives 2, < 16000 gives 1), MQTT and WS
survival, a web UI page probe under load, and the load script's own request rate. A reboot, a heap floor
breach or a cap that does not read back ends that arm; the cap is set back to 2 at the end.
The decision rule is evaluate_decision() (RULE below); it is printed with the result. Needs a
prerelease build with the restcap override (the readback fails on a stable build).

  python scripts/loadtest_sweep.py --restcap --host 192.168.88.61 --out-dir <dir>
"""

import argparse
import hashlib
import http.client
import json
import math
import os
import socket
import subprocess
import sys
import threading
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


# ---------------------------------------------------------------------------------------------
# TASK-3091: REST in-flight cap sweep (--restcap)
# ---------------------------------------------------------------------------------------------

RESTCAP_MIN, RESTCAP_MAX = 2, 8
TIER_FULL_BLOCK, TIER_CAP2_BLOCK = 24000, 16000   # restEffectiveInflightCap() in restAPI.ino
LOAD_PATHS = ["/api/v2/device/info", "/api/v2/sat/status", "/api/v2/settings", "/api/v2/health"]
UI_PROBE_PATH = "/v2.html"

# The decision rule, confirmed by otgw-architect 2026-11-26 BEFORE any run (TASK-3091 AC2,
# OTGW-field-data/_comms/rest_cap_decision_rule.md, 9 points). Base = the pooled N=2 arms at offered 8, which are
# interleaved between the other arms (point 2). Point numbers below refer to that file.
RULE = {
    "source": "rest_cap_decision_rule.md (otgw-architect, 2026-11-26)",
    "offered": 8,                           # (1) fixed offered load, above every N, so a 503 can occur at all
    "offered_realistic": 4,                 # (1) one realistic arm per N: 2 browsers (ADR-184); reported, not judged
    "min_repeats": 3,                       # (2) >= 3 repeats of >= 60 s per arm
    "min_seconds": 60,
    "effective_cap_share": 0.95,            # (3) N counts only if it was the effective cap in >= 95 % of the samples
    "free_heap_vs_base": 0.75,              # (4) sampled freeheap minimum not more than 25 % worse than the N=2 arms
    "maxblk_vs_base": 0.75,                 # (4) the same for the sampled largest-block minimum
    "min_free_heap_floor": 8192,            # (4) hd_min_free_heap (all-time low, never reset): absolute floor, whole run
    "sample_share": 0.8,                    # (4) heap numbers judged only if >= 80 % of the 1 Hz samples arrived
    "max_reboots": 0,                       # (5) 0 reboots or crashes
    "max_crashlogs": 0,                     # (5) 0 new crashlogs
    "max_ws_disconnects": 0,                # (5) 0 WS disconnects
    "max_mqtt_down_samples": 0,             # (5) 0 MQTT disconnects
    "p99_vs_base": 2.0,                     # (5) p99 (200 answers) at most 2x the N=2 arms
    "fail503_vs_base": 0.70,                # (5) at least 30 percent fewer client-visible 503 than N=2, at offered 8
}


def effective_cap(override, maxblk):
    """restEffectiveInflightCap() of the firmware (restAPI.ino): the override, clamped by the heap tier."""
    if maxblk is None:
        return None
    if maxblk < TIER_CAP2_BLOCK:
        return 1
    if maxblk < TIER_FULL_BLOCK:
        return min(override, 2)
    return override


def percentile(values, p):
    """Nearest-rank percentile of a list (None when empty)."""
    if not values:
        return None
    v = sorted(values)
    return v[max(0, math.ceil(p / 100.0 * len(v)) - 1)]


class Conn:
    """One keep-alive HTTP connection; request() returns (status, body, error). status -1 = transport error."""

    def __init__(self, host, timeout):
        self.host, self.timeout, self.c = host, timeout, None

    def close(self):
        if self.c is not None:
            try:
                self.c.close()
            except OSError:
                pass
            self.c = None

    def request(self, method, path, body=None):
        try:
            if self.c is None:
                self.c = http.client.HTTPConnection(self.host, timeout=self.timeout)
            headers, data = {}, None
            if body is not None:
                data = json.dumps(body).encode()
                headers["Content-Type"] = "application/json"
            self.c.request(method, path, body=data, headers=headers)
            r = self.c.getresponse()
            payload = r.read()
            if (r.getheader("Connection") or "").lower() == "close":
                self.close()
            return r.status, payload, None
        except (OSError, http.client.HTTPException) as exc:
            self.close()
            return -1, b"", type(exc).__name__


def device_dict(payload):
    try:
        j = json.loads(payload.decode())
    except (ValueError, UnicodeDecodeError):
        return {}
    d = j.get("device") if isinstance(j, dict) else None
    return d if isinstance(d, dict) else (j if isinstance(j, dict) else {})


def find_key(obj, key):
    """First value for key anywhere in nested dicts (the restcap field sits beside the flags object)."""
    if isinstance(obj, dict):
        if key in obj:
            return obj[key]
        for v in obj.values():
            r = find_key(v, key)
            if r is not None:
                return r
    return None


def get_restcap(conn):
    status, payload, _ = conn.request("GET", "/api/v2/debug/flags")
    if status != 200:
        return None
    try:
        v = find_key(json.loads(payload.decode()), "restcap")
    except (ValueError, UnicodeDecodeError):
        return None
    return v if isinstance(v, int) and not isinstance(v, bool) else None


def set_restcap(conn, n):
    """Set the override and read it back. Returns the value the device reports (None when it has none)."""
    status, _, _ = conn.request("POST", "/api/v2/debug/flags", {"name": "restcap", "value": str(n)})
    if status != 200:
        return None
    return get_restcap(conn)


def telnet_reset(host, port=23, timeout=5.0):
    """Telnet 'z': zero min_max_block, the histogram and the hwm counters (min free heap is not resettable)."""
    try:
        with socket.create_connection((host.split(":")[0], port), timeout=timeout) as s:
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


SAMPLE_KEYS = ["freeheap", "maxfreeblock", "mqttconnected", "bootcount", "hd_min_free_heap", "hd_min_max_block",
               "hd_rest_503", "hd_rest_inflight_hwm", "hd_ws_disconnects", "hd_mqtt_drops"]


def take_sample(conn, t_rel, retries=8):
    """One device/info sample. The sampler goes through the same REST gate as the load, so at a low cap it is
    shed with 503 when the load fills the slots: retry a few times (a 503 says the gate was full, not the heap)."""
    status, payload, err = conn.request("GET", "/api/v2/device/info")
    for _ in range(retries):
        if status != 503:
            break
        time.sleep(0.02)
        status, payload, err = conn.request("GET", "/api/v2/device/info")
    s = {"t": round(t_rel, 3), "status": status, "error": err}
    if status == 200:
        d = device_dict(payload)
        for k in SAMPLE_KEYS:
            s[k] = d.get(k)
    return s


class Run:
    """One timed run: the load clients, the sampler (with a UI page probe), and the optional WS watcher."""

    def __init__(self, host, offered, seconds, interval, timeout, think_s, ui_interval, heap_abort, ws_factory):
        self.host, self.offered, self.seconds = host, offered, seconds
        self.interval, self.timeout, self.think_s = interval, timeout, think_s
        self.ui_interval, self.heap_abort, self.ws_factory = ui_interval, heap_abort, ws_factory
        self.requests, self.samples, self.ui_probes = [], [], []
        self.stop = threading.Event()
        self.abort_reason = None
        self.ws = None
        self._lock, self._next = threading.Lock(), 0
        self.t0 = None

    def _client(self, t_end):
        c = Conn(self.host, self.timeout)
        while time.monotonic() < t_end and not self.stop.is_set():
            with self._lock:
                i = self._next
                self._next += 1
            path = LOAD_PATHS[i % len(LOAD_PATHS)]
            t = time.monotonic()
            status, _, err = c.request("GET", path)
            self.requests.append((round(t - self.t0, 3), path, status, round((time.monotonic() - t) * 1000.0, 2), err))
            if self.think_s:
                self.stop.wait(self.think_s)
        c.close()

    def _sampler(self, t_end):
        c, last_ui = Conn(self.host, self.timeout), -1e9
        while True:
            now = time.monotonic()
            s = take_sample(c, now - self.t0)
            self.samples.append(s)
            free = s.get("freeheap")
            if self.heap_abort and isinstance(free, int) and free < self.heap_abort and not self.stop.is_set():
                self.abort_reason = f"freeheap {free} below the abort floor {self.heap_abort}"
                self.stop.set()
            if self.ui_interval and now - last_ui >= self.ui_interval:
                last_ui = now
                t = time.monotonic()
                st, _, err = c.request("GET", UI_PROBE_PATH)
                self.ui_probes.append({"t": round(t - self.t0, 3), "status": st,
                                       "ms": round((time.monotonic() - t) * 1000.0, 2), "error": err})
            if self.stop.wait(self.interval) or time.monotonic() >= t_end:
                break
        c.close()

    def go(self):
        self.t0 = time.monotonic()
        t_end = self.t0 + self.seconds
        ws = self.ws_factory(self.host) if self.ws_factory else None
        if ws:
            ws.start()
        threads = [threading.Thread(target=self._sampler, args=(t_end,), daemon=True)]
        threads += [threading.Thread(target=self._client, args=(t_end,), daemon=True) for _ in range(self.offered)]
        for t in threads:
            t.start()
        for t in threads:
            t.join(timeout=self.seconds + self.timeout + 5)
        self.stop.set()
        self.ws = ws.stop() if ws else None


try:
    import websocket  # websocket-client, optional
except ImportError:
    websocket = None


class WsWatcher:
    """Optional: one /ws client for the run; counts frames and whether the socket dropped. Needs websocket-client."""

    def __init__(self, host):
        self.url, self.frames, self.dropped, self.connected = f"ws://{host}/ws", 0, False, False
        self._stop, self._thread = threading.Event(), None

    def _loop(self):
        try:
            ws = websocket.create_connection(self.url, timeout=10)
            self.connected = True
        except Exception:
            return
        ws.settimeout(1.0)
        while not self._stop.is_set():
            try:
                ws.recv()
                self.frames += 1
            except websocket.WebSocketTimeoutException:
                continue
            except Exception:
                self.dropped = True
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
        return {"connected": self.connected, "frames": self.frames, "dropped": self.dropped}


def get_crashlog(conn):
    """(available, summary) of /api/v2/device/crashlog, or None when it cannot be read."""
    status, payload, _ = conn.request("GET", "/api/v2/device/crashlog")
    if status != 200:
        return None
    try:
        c = json.loads(payload.decode()).get("crashlog", {})
    except (ValueError, UnicodeDecodeError, AttributeError):
        return None
    return bool(c.get("available")), str(c.get("summary", ""))


def new_crashlog(before, after):
    """A crashlog that was not there before the run, or whose summary changed."""
    if after is None or not after[0]:
        return False
    return before is None or not before[0] or before[1] != after[1]


def summarize_repeat(arm, n, offered, rnd, run, before, after, readback, seconds, interval, crash_before, crash_after):
    """Turn one run into the per-repeat row (pure: only the data given)."""
    ok = [r for r in run.requests if r[2] == 200]
    shed = [r for r in run.requests if r[2] == 503]
    other = [r for r in run.requests if r[2] not in (200, 503)]
    lat_ok = [r[3] for r in ok]
    lat_all = [r[3] for r in run.requests if r[2] > 0]
    good = [s for s in run.samples if s["status"] == 200]
    free = [s["freeheap"] for s in good if isinstance(s.get("freeheap"), int)]
    blk = [s["maxfreeblock"] for s in good if isinstance(s.get("maxfreeblock"), int)]
    eff = [effective_cap(n, b) for b in blk]
    boot_before, boot_after = before.get("bootcount"), after.get("bootcount")
    seen = {s.get("bootcount") for s in good if s.get("bootcount") is not None}
    reboots = 0
    if boot_before is not None:
        reboots = len(seen - {boot_before})
        if boot_after is not None:
            reboots = max(reboots, boot_after - boot_before)
    mqtt_down = 0 if before.get("mqttconnected") is False else sum(1 for s in good if s.get("mqttconnected") is False)

    def delta(key):
        a, b = before.get(key), after.get(key)
        return (b - a) if isinstance(a, int) and isinstance(b, int) else None

    expected = max(1, round(seconds / interval))
    own = len(run.requests) + len(run.samples) + len(run.ui_probes)
    return {
        "arm": arm, "n": n, "offered": offered, "round": rnd, "seconds": seconds,
        "aborted": run.abort_reason,
        "requests": {"total": len(run.requests), "ok": len(ok), "r503": len(shed), "other_failures": len(other),
                     "other_detail": sorted({str(r[4] or r[2]) for r in other}),
                     "share_503": (len(shed) / len(run.requests)) if run.requests else None},
        "latency_ms_ok": {"p50": percentile(lat_ok, 50), "p95": percentile(lat_ok, 95),
                          "p99": percentile(lat_ok, 99), "max": max(lat_ok) if lat_ok else None},
        "latency_ms_all": {"p50": percentile(lat_all, 50), "p95": percentile(lat_all, 95), "p99": percentile(lat_all, 99)},
        "own_load": {"load_rps": round(len(run.requests) / seconds, 2), "sampler_rps": round(len(run.samples) / seconds, 2),
                     "total_rps": round(own / seconds, 2), "sampler_503": sum(1 for s in run.samples if s["status"] == 503),
                     "sampler_failed": sum(1 for s in run.samples if s["status"] != 200)},
        "heap": {"freeheap_min_sampled": min(free) if free else None, "maxblk_min_sampled": min(blk) if blk else None,
                 "maxblk_p50_sampled": percentile(blk, 50), "samples": len(good), "samples_expected": expected,
                 "sample_share": len(good) / expected,
                 "hd_min_free_heap_before": before.get("hd_min_free_heap"),
                 "hd_min_free_heap_after": after.get("hd_min_free_heap"),
                 "hd_min_max_block_after": after.get("hd_min_max_block")},
        "effective_cap": {"override": n, "readback": readback, "min": min(eff) if eff else None,
                          "share_at_n": (sum(1 for e in eff if e == n) / len(eff)) if eff else None,
                          "device_inflight_hwm_after": after.get("hd_rest_inflight_hwm")},
        "survival": {"reboots": reboots, "bootcount_before": boot_before, "bootcount_after": boot_after,
                     "crashlogs": 1 if new_crashlog(crash_before, crash_after) else 0,
                     "mqtt_down_samples": mqtt_down, "ws_disconnects_device": delta("hd_ws_disconnects"),
                     "ws_client": run.ws if isinstance(run.ws, dict) else None, "mqtt_drops_delta": delta("hd_mqtt_drops")},
        "device_rest_503_delta": delta("hd_rest_503"),
        "ui_probe": {"count": len(run.ui_probes), "failed": sum(1 for p in run.ui_probes if p["status"] != 200),
                     "max_ms": max((p["ms"] for p in run.ui_probes), default=None)},
    }


def aggregate_arm(n, reps):
    """Pool the repeats of one arm (one N at one offered load) into the figures the decision rule reads."""
    done = [r for r in reps if not r.get("error")]
    tot = sum(r["requests"]["total"] for r in done)
    r503 = sum(r["requests"]["r503"] for r in done)
    lat = [x for r in done for x in r.get("_lat_ok", [])]
    free = [r["heap"]["freeheap_min_sampled"] for r in done if r["heap"]["freeheap_min_sampled"] is not None]
    blk = [r["heap"]["maxblk_min_sampled"] for r in done if r["heap"]["maxblk_min_sampled"] is not None]
    wm = [r["heap"]["hd_min_free_heap_after"] for r in done if r["heap"]["hd_min_free_heap_after"] is not None]
    shares = [r["effective_cap"]["share_at_n"] for r in done if r["effective_cap"]["share_at_n"] is not None]
    ws_dev = sum(r["survival"]["ws_disconnects_device"] or 0 for r in done)
    ws_cli = sum(1 for r in done if (r["survival"]["ws_client"] or {}).get("dropped"))
    return {
        "n": n, "offered": reps[0]["offered"],
        "repeats_run": len(done), "repeats_errored": len(reps) - len(done),
        "errors": sorted({r["error"] for r in reps if r.get("error")}),
        "aborted": [r["aborted"] for r in done if r.get("aborted")],
        "requests": tot, "r503": r503, "share_503": (r503 / tot) if tot else None,
        "other_failures": sum(r["requests"]["other_failures"] for r in done),
        "p95_ms_ok": percentile(lat, 95), "p99_ms_ok": percentile(lat, 99),
        "freeheap_min_sampled": min(free) if free else None,
        "maxblk_min_sampled": min(blk) if blk else None,
        "sample_share_min": min((r["heap"]["sample_share"] for r in done), default=0.0),
        "hd_min_free_heap_after_min": min(wm) if wm else None,
        "effective_cap_share_at_n": min(shares) if shares else None,
        "reboots": sum(r["survival"]["reboots"] for r in done),
        "crashlogs": sum(r["survival"]["crashlogs"] for r in done),
        "mqtt_down_samples": sum(r["survival"]["mqtt_down_samples"] for r in done),
        "ws_disconnects": ws_dev + ws_cli,
        "load_rps_mean": round(sum(r["own_load"]["load_rps"] for r in done) / len(done), 2) if done else None,
        "ui_probe_failed": sum(r["ui_probe"]["failed"] for r in done),
    }


def aggregate_control(reps):
    done = [r for r in reps if not r.get("error")]
    if not done:
        return None
    free = [r["heap"]["freeheap_min_sampled"] for r in done if r["heap"]["freeheap_min_sampled"] is not None]
    return {"repeats_run": len(done), "freeheap_min_sampled": min(free) if free else None,
            "sampler_rps_mean": round(sum(r["own_load"]["sampler_rps"] for r in done) / len(done), 2),
            "reboots": sum(r["survival"]["reboots"] for r in done)}


def evaluate_decision(arms, rule=None, regression=False):
    """arms: {n: aggregate_arm(...)} at the judged offered load, including the pooled base arm 2.
    Returns per-N criteria and the new default. regression=True (for the unit that does not decide, .61) judges
    only criteria 1-3 of the rule (point 6): no 503 criterion and no new default."""
    rule = rule or RULE
    base = arms.get(RESTCAP_MIN)
    out = {"rule": rule, "base_n": RESTCAP_MIN, "regression_only": regression, "per_n": {}, "new_default": RESTCAP_MIN,
           "reason": None}
    if not base or not base["repeats_run"]:
        out["reason"] = "no usable N=2 base arm"
        return out
    passing = []
    for n in sorted(arms):
        if n == RESTCAP_MIN:
            continue
        a, c = arms[n], {}
        c["repeats"] = a["repeats_run"] >= rule["min_repeats"] and not a["repeats_errored"]
        c["3_effective_cap"] = bool(a["effective_cap_share_at_n"] is not None
                                    and a["effective_cap_share_at_n"] >= rule["effective_cap_share"])
        c["1_survival"] = (a["reboots"] <= rule["max_reboots"] and a["crashlogs"] <= rule["max_crashlogs"]
                           and a["ws_disconnects"] <= rule["max_ws_disconnects"]
                           and a["mqtt_down_samples"] <= rule["max_mqtt_down_samples"] and not a["aborted"])
        wm = a["hd_min_free_heap_after_min"]
        floor_ok = wm is not None and wm >= rule["min_free_heap_floor"]
        enough = (a["sample_share_min"] >= rule["sample_share"] and base["sample_share_min"] >= rule["sample_share"])
        fb, fa, bb, ba = base["freeheap_min_sampled"], a["freeheap_min_sampled"], base["maxblk_min_sampled"], a["maxblk_min_sampled"]
        rel_free = fb is not None and fa is not None and fa >= rule["free_heap_vs_base"] * fb
        rel_blk = bb is not None and ba is not None and ba >= rule["maxblk_vs_base"] * bb
        c["2_heap"] = bool(floor_ok and enough and rel_free and rel_blk)
        c["p99"] = bool(a["p99_ms_ok"] is not None and base["p99_ms_ok"] is not None
                        and a["p99_ms_ok"] <= rule["p99_vs_base"] * base["p99_ms_ok"])
        if not regression:
            bs, ns = base["share_503"], a["share_503"]
            c["503"] = bool(bs and ns is not None and ns <= rule["fail503_vs_base"] * bs)   # no 503 at N=2: nothing to reduce
        out["per_n"][n] = {"criteria": c, "pass": all(c.values())}
        if all(c.values()):
            passing.append(n)
    if regression:
        out["new_default"] = None
        out["reason"] = "regression check only: the largest passing N is decided on the PSRAM unit (rule point 6)"
    elif passing:
        out["new_default"] = max(passing)
        out["reason"] = f"N={max(passing)} is the largest arm that passes every criterion on this unit"
    else:
        out["reason"] = "no arm passes every criterion: the default stays 2 (per_n shows the failing criteria)"
    return out


def build_round(sweep, offered, realistic, control, rnd):
    """One round of arms as (label, n, offered). The pooled base arm N=2 at the judged offered load runs between all
    others (rule point 2: 2,4,2,6,2,8,...), so slow heap drift is not credited to an N. The other arms rotate."""
    base = (f"{RESTCAP_MIN}@{offered}", RESTCAP_MIN, offered)
    groups = []
    for n in sweep:
        if n == RESTCAP_MIN:
            if realistic:
                groups.append([(f"{n}@{realistic}", n, realistic)])
            continue
        g = [(f"{n}@{offered}", n, offered)]
        if realistic:
            g.append((f"{n}@{realistic}", n, realistic))
        groups.append(g)
    if control:
        groups.append([("control", RESTCAP_MIN, 0)])
    k = (rnd - 1) % len(groups) if groups else 0
    seq = [base]
    for g in groups[k:] + groups[:k]:
        seq.extend(g)
        seq.append(base)
    return seq


def wait_alive(host, timeout, limit):
    end = time.monotonic() + limit
    c = Conn(host, timeout)
    while True:
        status, _, _ = c.request("GET", "/api/v2/health")
        if status == 200:
            return True
        if time.monotonic() >= end:
            return False
        time.sleep(min(2.0, max(0.05, limit / 5.0)))


def run_one(args, host, conn, arm, n, offered, rnd, ws_factory):
    """Set the cap (read it back), reset the device counters, run the load, read the counters again."""
    status, payload, _ = conn.request("GET", "/api/v2/device/info")
    if status != 200 and not wait_alive(host, args.timeout, args.recover_wait):
        return {"arm": arm, "n": n, "offered": offered, "round": rnd, "error": "device_unreachable"}
    readback = set_restcap(conn, n)
    if readback != n:
        return {"arm": arm, "n": n, "offered": offered, "round": rnd,
                "error": f"cap_not_applied (asked {n}, device reports {readback})"}
    if not args.no_reset:
        telnet_reset(host, args.telnet_port)
    time.sleep(args.settle)
    status, payload, _ = conn.request("GET", "/api/v2/device/info")
    before = device_dict(payload)
    crash_before = get_crashlog(conn)
    run = Run(host, offered, args.seconds, args.sample_interval, args.timeout, args.think_ms / 1000.0,
              args.ui_probe_interval, args.heap_abort, ws_factory)
    run.go()
    alive = wait_alive(host, args.timeout, args.recover_wait)
    status, payload, _ = conn.request("GET", "/api/v2/device/info")
    after = device_dict(payload) if alive else {}
    crash_after = get_crashlog(conn) if alive else None
    row = summarize_repeat(arm, n, offered, rnd, run, before, after, readback, args.seconds, args.sample_interval,
                           crash_before, crash_after)
    row["_lat_ok"] = [r[3] for r in run.requests if r[2] == 200]
    if not alive:
        row["error"] = "device_not_back_after_run"
    if args.out_dir:
        with open(os.path.join(args.out_dir, f"requests-{arm}-r{rnd}.ndjson"), "w", encoding="utf-8") as fh:
            for r in run.requests:
                fh.write(json.dumps({"t": r[0], "path": r[1], "status": r[2], "ms": r[3], "err": r[4]}) + "\n")
    return row


def restcap_main(args):
    host = args.host
    sweep = [int(x) for x in args.sweep.split(",")] if args.sweep else list(range(RESTCAP_MIN, RESTCAP_MAX + 1))
    if RESTCAP_MIN not in sweep:
        sweep = [RESTCAP_MIN] + sweep
    realistic = 0 if args.no_realistic else args.offered_realistic
    ws_factory = WsWatcher if (websocket is not None and not args.no_ws) else None
    conn = Conn(host, args.timeout)
    if get_restcap(conn) is None:
        print(f"REFUSED: {host} reports no restcap override (stable build, or a prerelease from before TASK-3091)", file=sys.stderr)
        return 2
    if args.out_dir:
        os.makedirs(args.out_dir, exist_ok=True)
    rows, skip_n = [], set()

    def write_out(final):
        by_arm = {}
        for r in rows:
            by_arm.setdefault(r["arm"], []).append(r)
        judged = {r["n"]: aggregate_arm(r["n"], by_arm[r["arm"]]) for r in rows
                  if r["arm"] != "control" and r["offered"] == args.offered}
        # pooled base arm: every N=2 repeat at the judged offered load, over the whole interleaved schedule
        agg = {f"{a['n']}@{a['offered']}": a for a in judged.values()}
        real = {a: aggregate_arm(reps[0]["n"], reps) for a, reps in by_arm.items()
                if a != "control" and reps[0]["offered"] != args.offered}
        result = {"host": host, "sweep": sweep, "offered": args.offered, "offered_realistic": realistic,
                  "seconds": args.seconds, "repeats": args.repeats, "complete": final,
                  "arms": agg, "realistic_arms": real,
                  "control": aggregate_control(by_arm.get("control", [])),
                  "decision": evaluate_decision(judged, regression=args.regression),
                  "repeats_detail": [{k: v for k, v in r.items() if not k.startswith("_")} for r in rows]}
        if args.out_dir:
            path = os.path.join(args.out_dir, "sweep.json")
            with open(path, "w", encoding="utf-8") as fh:
                json.dump(result, fh, indent=2, default=str)
            with open(path, "rb") as fh:
                digest = hashlib.sha256(fh.read()).hexdigest()
            with open(os.path.join(args.out_dir, "SHA256SUMS"), "w", encoding="utf-8") as fh:
                fh.write(f"{digest}  sweep.json\n")
        return result

    try:
        for rnd in range(1, args.repeats + 1):
            for label, n, offered in build_round(sweep, args.offered, realistic, not args.no_control, rnd):
                if label != "control" and n in skip_n:
                    continue
                arm = f"{label}"
                row = run_one(args, host, conn, arm, n, offered, rnd, ws_factory)
                rows.append(row)
                print(f"round {rnd} arm {arm}: " + (row.get("error") or
                      f"503 {row['requests']['r503']}/{row['requests']['total']} p99 {row['latency_ms_ok']['p99']} "
                      f"reboots {row['survival']['reboots']}"), file=sys.stderr)
                if label != "control" and (row.get("error") or row.get("aborted") or row["survival"]["reboots"]
                                           or row["survival"]["crashlogs"]):
                    skip_n.add(n)
                write_out(False)
    finally:
        for _ in range(3):
            if set_restcap(Conn(host, args.timeout), RESTCAP_MIN) == RESTCAP_MIN:
                break
    result = write_out(True)
    print(json.dumps({k: result[k] for k in ("arms", "realistic_arms", "control", "decision")}, indent=2, default=str))
    return 0


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--host", required=True, help="Device IP, e.g. 192.168.88.64")
    p.add_argument("--scenario-seconds", type=float, default=10.0, help="Duration for each of S3/S4/S5 per arm")
    p.add_argument("--arm-duration", type=float, default=None, help="capture-mqtt-debug.bat window per arm (default: auto, covers all scenarios + margin)")
    p.add_argument("--output-root", default="logs/loadtest-sweep-phase1", help="Root dir for per-arm transcripts")
    p.add_argument("--sweep", default=None, help="Comma-separated offered-N list (default: 1,2,3,4,6,8)")
    r = p.add_argument_group("REST in-flight cap sweep (TASK-3091), with --restcap")
    r.add_argument("--restcap", action="store_true", help="sweep the REST in-flight cap (restcap override) at a fixed offered load")
    r.add_argument("--offered", type=int, default=8, help="judged offered clients per arm, fixed over the arms (default 8)")
    r.add_argument("--offered-realistic", type=int, default=4, help="extra arm per N at this offered load, reported only (default 4)")
    r.add_argument("--no-realistic", action="store_true", help="skip the realistic offered-4 arms")
    r.add_argument("--regression", action="store_true", help="judge criteria 1-3 only (the unit that does not decide, .61)")
    r.add_argument("--seconds", type=float, default=60.0, help="seconds per repeat (default 60)")
    r.add_argument("--repeats", type=int, default=3, help="rounds; every arm runs once per round (default 3)")
    r.add_argument("--no-control", action="store_true", help="skip the no-load control arm")
    r.add_argument("--think-ms", type=float, default=20.0, help="pause of a client between two requests (default 20)")
    r.add_argument("--sample-interval", type=float, default=1.0, help="device/info sampler period in s (default 1)")
    r.add_argument("--ui-probe-interval", type=float, default=10.0, help="GET /v2.html under load every s (0 = off)")
    r.add_argument("--timeout", type=float, default=10.0, help="per-request timeout in s")
    r.add_argument("--settle", type=float, default=2.0, help="seconds between the cap change and the run")
    r.add_argument("--heap-abort", type=int, default=6000, help="end the run when sampled freeheap drops below this (0 = off)")
    r.add_argument("--recover-wait", type=float, default=120.0, help="seconds to wait for the device after a run or reboot")
    r.add_argument("--telnet-port", type=int, default=23)
    r.add_argument("--no-reset", action="store_true", help="skip the telnet 'z' counter reset before a repeat")
    r.add_argument("--no-ws", action="store_true", help="no /ws watcher client")
    r.add_argument("--out-dir", default=None, help="write sweep.json, per-repeat request logs and SHA256SUMS here")
    args = p.parse_args()

    if args.restcap:
        sys.exit(restcap_main(args))

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
