#!/usr/bin/env python3
"""TASK-3091 AC3: the REST in-flight cap sweep in scripts/loadtest_sweep.py (--restcap), against a fake device.

The fake device is a threaded HTTP/1.1 server with the firmware's gate: an /api request is answered 503 when the
in-flight count has reached the effective cap = min(override, heap tier), like restEffectiveInflightCap() in
restAPI.ino (tier: maxblk < 24000 gives 2, < 16000 gives 1). It serves GET/POST /api/v2/debug/flags with the
restcap override, /api/v2/device/info with the counters, and can reboot, run low on heap, or ignore the override.

    python tests/test_loadtest_restcap_sweep.py
"""
import contextlib
import hashlib
import io
import json
import os
import socket
import subprocess
import sys
import tempfile
import threading
import time
import unittest
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from types import SimpleNamespace
from unittest import mock

REPO = Path(__file__).resolve().parent.parent
SCRIPT = os.environ.get("LOADTEST_SWEEP_SCRIPT", str(REPO / "scripts" / "loadtest_sweep.py"))
sys.path.insert(0, str(Path(SCRIPT).parent))
import importlib.util

_spec = importlib.util.spec_from_file_location("loadtest_sweep_under_test", SCRIPT)
sweep = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(sweep)


class FakeDevice:
    """Mutable device state plus the server that exposes it."""

    def __init__(self, **kw):
        self.cap = 2                  # the override (RAM only: a reboot sets it back to 2)
        self.has_restcap = True
        self.ignore_restcap = False
        self.maxblk = 40000
        self.freeheap = 90000
        self.hd_min_free_heap = 60000
        self.bootcount = 7
        self.mqtt = True
        self.ws_disc = 0
        self.delay = 0.03
        self.reboot_when_cap = None   # reboot once, 15 gated requests into a run with this cap
        self.reboot_outage = 0.4
        self.down_until = 0.0
        self.inflight = 0
        self.inflight_hwm = 0
        self.rest503 = 0
        self.gated = 0
        self.since_cap = 0
        self.rebooted = False
        self.crashlog = None          # None, or a summary string once the device has a crashlog
        self.lock = threading.Lock()
        self.__dict__.update(kw)

    def effective_cap(self):
        if self.maxblk < 16000:
            return 1
        if self.maxblk < 24000:
            return min(self.cap, 2)
        return self.cap

    def info(self):
        return {"device": {"freeheap": self.freeheap, "maxfreeblock": self.maxblk, "mqttconnected": self.mqtt,
                           "bootcount": self.bootcount, "hd_min_free_heap": self.hd_min_free_heap,
                           "hd_min_max_block": self.maxblk, "hd_rest_503": self.rest503,
                           "hd_rest_inflight_hwm": self.inflight_hwm, "hd_ws_disconnects": self.ws_disc,
                           "hd_mqtt_drops": 0}}


def make_handler(dev):
    class H(BaseHTTPRequestHandler):
        protocol_version = "HTTP/1.1"

        def log_message(self, *a):
            pass

        def send(self, code, obj=None, raw=b"<html></html>"):
            body = json.dumps(obj).encode() if obj is not None else raw
            self.send_response(code)
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)

        def do_POST(self):
            n = int(self.headers.get("Content-Length") or 0)
            body = json.loads(self.rfile.read(n) or b"{}")
            if self.path == "/api/v2/debug/flags" and dev.has_restcap:
                if not dev.ignore_restcap and body.get("name") == "restcap":
                    dev.cap = int(body["value"])
                    dev.since_cap = 0
                return self.send(200, {"ok": True})
            self.send(404, {"error": "no"})

        def do_GET(self):
            now = time.monotonic()
            if now < dev.down_until:
                self.close_connection = True
                return
            if self.path == "/api/v2/device/crashlog":
                return self.send(200, {"crashlog": {"available": dev.crashlog is not None, "summary": dev.crashlog or ""}})
            if self.path == "/api/v2/debug/flags":
                if not dev.has_restcap:
                    return self.send(200, {"flags": {"debugdummy": False}})
                return self.send(200, {"flags": {"debugdummy": False}, "restcap": dev.cap})
            if self.path.startswith("/api/"):
                with dev.lock:
                    dev.gated += 1
                    dev.since_cap += 1
                    if dev.reboot_when_cap == dev.cap and not dev.rebooted and dev.since_cap > 15:
                        dev.rebooted, dev.bootcount, dev.cap = True, dev.bootcount + 1, 2
                        dev.down_until = now + dev.reboot_outage
                        self.close_connection = True
                        return
                    if dev.inflight >= dev.effective_cap():
                        dev.rest503 += 1
                        return self.send(503, {"error": "busy"})
                    dev.inflight += 1
                    dev.inflight_hwm = max(dev.inflight_hwm, dev.inflight)
                time.sleep(dev.delay)
                with dev.lock:
                    dev.inflight -= 1
                if self.path == "/api/v2/device/info":
                    return self.send(200, dev.info())
                return self.send(200, {"ok": True, "path": self.path})
            return self.send(200)

    return H


class Server:
    def __init__(self, dev):
        self.dev = dev
        self.httpd = ThreadingHTTPServer(("127.0.0.1", 0), make_handler(dev))
        self.httpd.daemon_threads = True
        self.host = f"127.0.0.1:{self.httpd.server_address[1]}"
        threading.Thread(target=self.httpd.serve_forever, daemon=True).start()

    def close(self):
        self.httpd.shutdown()
        self.httpd.server_close()


def args(host, **kw):
    a = dict(host=host, sweep="2,6", restcap=True, offered=6, offered_realistic=3, no_realistic=True, regression=False,
             seconds=1.0, repeats=2, no_control=False, think_ms=0.0,
             sample_interval=0.15, ui_probe_interval=0.5, timeout=3.0, settle=0.0, heap_abort=6000, recover_wait=5.0,
             telnet_port=1, no_reset=True, no_ws=True, out_dir=None)
    a.update(kw)
    return SimpleNamespace(**a)


class Base(unittest.TestCase):
    def start(self, **kw):
        self.dev = FakeDevice(**kw)
        self.srv = Server(self.dev)
        self.addCleanup(self.srv.close)
        return self.srv.host

    def sweep(self, host, **kw):
        """Run restcap_main and return the final sweep.json content."""
        d = tempfile.mkdtemp()
        self.addCleanup(lambda: __import__("shutil").rmtree(d, ignore_errors=True))
        with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
            rc = sweep.restcap_main(args(host, out_dir=d, **kw))
        self.rc = rc
        p = Path(d, "sweep.json")
        return json.loads(p.read_text(encoding="utf-8")) if p.exists() else None, d


class Sweep(Base):
    def test_a_higher_cap_sheds_fewer_503_and_the_cap_is_restored(self):
        host = self.start()
        res, d = self.sweep(host)
        self.assertEqual(self.rc, 0)
        a2, a6 = res["arms"]["2@6"], res["arms"]["6@6"]
        self.assertEqual((a2["repeats_run"], a6["repeats_run"]), (6, 2), "N=2 runs between every other arm and is pooled")
        self.assertGreater(a2["share_503"], 0.2, "6 clients against cap 2 are shed")
        self.assertLess(a6["share_503"], a2["share_503"] * 0.5, "cap 6 sheds far less")
        self.assertEqual(a6["effective_cap_share_at_n"], 1.0)
        self.assertEqual(self.dev.cap, 2, "the override is set back to 2 at the end")
        self.assertGreaterEqual(self.dev.inflight_hwm, 6, "the device really saw 6 in flight")

    def test_the_cap_is_restored_when_the_sweep_is_interrupted_with_a_high_cap(self):
        host = self.start()
        real = sweep.run_one
        calls = []

        def interrupted(a, h, conn, arm, n, offered, rnd, ws):
            calls.append(arm)
            row = real(a, h, conn, arm, n, offered, rnd, ws)
            if arm == "6@6":
                self.dev.cap = 6                       # still raised when the interrupt arrives
                raise KeyboardInterrupt
            return row

        with mock.patch.object(sweep, "run_one", interrupted):
            with self.assertRaises(KeyboardInterrupt):
                self.sweep(host, repeats=1, sweep="2,6", no_control=True)
        self.assertIn("6@6", calls)
        self.assertEqual(self.dev.cap, 2, "the finally block sets the override back to 2")

    def test_request_classes_add_up(self):
        host = self.start()
        res, d = self.sweep(host, repeats=1, sweep="2", no_control=True)
        r = res["repeats_detail"][0]["requests"]
        self.assertGreater(r["r503"], 0)
        self.assertEqual(r["ok"] + r["r503"] + r["other_failures"], r["total"])

    def test_control_arm_has_no_load_and_still_samples(self):
        host = self.start()
        res, d = self.sweep(host)
        rows = [r for r in res["repeats_detail"] if r["arm"] == "control"]
        self.assertEqual(len(rows), 2)
        for r in rows:
            self.assertEqual(r["requests"]["total"], 0)
            self.assertGreater(r["own_load"]["sampler_rps"], 2.0, "the sampler (the perturbation floor) still runs")
        self.assertEqual(res["control"]["repeats_run"], 2)

    def test_the_base_arm_runs_between_all_others_and_the_rest_rotates(self):
        labels = lambda rnd: [x[0] for x in sweep.build_round([2, 4, 6], 8, 4, True, rnd)]
        self.assertEqual(labels(1), ["2@8", "2@4", "2@8", "4@8", "4@4", "2@8", "6@8", "6@4", "2@8", "control", "2@8"])
        self.assertEqual(labels(2), ["2@8", "4@8", "4@4", "2@8", "6@8", "6@4", "2@8", "control", "2@8", "2@4", "2@8"])
        for rnd in (1, 2, 3, 4):
            seq = labels(rnd)
            self.assertEqual(seq[0], "2@8")
            self.assertEqual(seq[-1], "2@8")
            self.assertTrue(all(seq[k] == "2@8" or seq[k - 1] == "2@8" or seq[k + 1] == "2@8" or seq[k].split("@")[0] == seq[k - 1].split("@")[0] or seq[k].split("@")[0] == seq[k + 1].split("@")[0] for k in range(1, len(seq) - 1)))
        self.assertEqual([x[2] for x in sweep.build_round([2, 4], 8, 4, True, 1) if x[0] == "control"], [0], "the control offers no load")
        self.assertEqual(sorted(set(labels(1))), sorted(set(labels(2))), "every round has every arm")

    def test_a_run_pools_the_base_arm_and_reports_realistic_arms_apart(self):
        host = self.start()
        res, d = self.sweep(host, repeats=1, sweep="2,4", no_control=True, no_realistic=False, offered=6, offered_realistic=2)
        order = [r["arm"] for r in res["repeats_detail"]]
        self.assertEqual(order, ["2@6", "2@2", "2@6", "4@6", "4@2", "2@6"])
        self.assertEqual(res["arms"]["2@6"]["repeats_run"], 3, "the three interleaved N=2 repeats are pooled")
        self.assertIn("4@6", res["arms"])
        self.assertNotIn("4@2", res["arms"])
        self.assertIn("4@2", res["realistic_arms"])
        self.assertIn("4", res["decision"]["per_n"], "only the judged offered load decides")

    def test_a_new_crashlog_counts_and_drops_the_arm(self):
        host = self.start()
        threading.Timer(0.5, lambda: setattr(self.dev, "crashlog", "Guru Meditation")).start()
        res, d = self.sweep(host, repeats=2, sweep="2,6", no_control=True)
        self.assertGreaterEqual(sum(a["crashlogs"] for a in res["arms"].values()), 1)

    def test_an_old_crashlog_that_does_not_change_is_not_new(self):
        host = self.start(crashlog="old crash")
        res, d = self.sweep(host, repeats=1, sweep="2,6", no_control=True)
        self.assertEqual(sum(a["crashlogs"] for a in res["arms"].values()), 0)

    def test_regression_mode_judges_no_503_criterion_and_gives_no_default(self):
        host = self.start()
        res, d = self.sweep(host, repeats=3, sweep="2,6", no_control=True, regression=True)
        dec = res["decision"]
        self.assertTrue(dec["regression_only"])
        self.assertIsNone(dec["new_default"])
        self.assertNotIn("503", dec["per_n"]["6"]["criteria"])

    def test_the_heap_tier_clamps_the_effective_cap(self):
        host = self.start(maxblk=20000)           # 16000 <= maxblk < 24000: the tier allows 2 whatever the override
        res, d = self.sweep(host)
        a6 = res["arms"]["6@6"]
        self.assertEqual(a6["effective_cap_share_at_n"], 0.0)
        row = [r for r in res["repeats_detail"] if r["arm"] == "6@6"][0]
        self.assertEqual(row["effective_cap"]["min"], 2)
        self.assertEqual(row["effective_cap"]["readback"], 6)
        self.assertEqual(row["heap"]["maxblk_min_sampled"], 20000)
        self.assertFalse(res["decision"]["per_n"]["6"]["criteria"]["3_effective_cap"])
        self.assertEqual(self.dev.inflight_hwm, 2, "the device never admitted more than 2")

    def test_a_cap_that_does_not_read_back_ends_the_arm_without_load(self):
        host = self.start(ignore_restcap=True)
        res, d = self.sweep(host, sweep="2,6", no_control=True)
        rows = [r for r in res["repeats_detail"] if r["arm"] == "6@6"]
        self.assertTrue(rows and all("cap_not_applied" in r["error"] for r in rows))
        self.assertEqual(res["arms"]["6@6"]["repeats_run"], 0)
        self.assertEqual([r for r in rows if "requests" in r], [], "no load row exists for the arm with the wrong cap")
        self.assertEqual(self.dev.inflight_hwm, 2, "the device never saw more than the base cap in flight")

    def test_a_stable_build_without_restcap_is_refused(self):
        host = self.start(has_restcap=False)
        res, d = self.sweep(host)
        self.assertEqual(self.rc, 2)
        self.assertIsNone(res)

    def test_a_reboot_is_counted_and_the_arm_is_dropped_from_later_rounds(self):
        host = self.start(reboot_when_cap=6)
        res, d = self.sweep(host, repeats=3, no_control=True)
        a6 = res["arms"]["6@6"]
        self.assertGreaterEqual(a6["reboots"], 1)
        self.assertLess(a6["repeats_run"] + a6["repeats_errored"], 3, "N=6 is skipped after the reboot")
        self.assertFalse(res["decision"]["per_n"]["6"]["criteria"]["1_survival"])
        self.assertEqual(res["decision"]["new_default"], 2)
        self.assertEqual(self.dev.cap, 2)

    def test_a_low_heap_ends_the_run_early(self):
        host = self.start(freeheap=5000)
        t = time.monotonic()
        res, d = self.sweep(host, seconds=3.0, repeats=1, sweep="2", no_control=True)
        self.assertLess(time.monotonic() - t, 2.5, "the run did not last the 3 s")
        row = res["repeats_detail"][0]
        self.assertIn("below the abort floor", row["aborted"])
        self.assertTrue(res["arms"]["2@6"]["aborted"])

    def test_mqtt_dropping_during_the_run_is_survival_failure(self):
        host = self.start()
        res0, _ = self.sweep(host, repeats=1, sweep="2,3", no_control=True, offered=1)
        self.assertEqual(res0["arms"]["3@1"]["mqtt_down_samples"], 0)
        threading.Timer(0.3, lambda: setattr(self.dev, "mqtt", False)).start()
        res, _ = self.sweep(host, repeats=1, sweep="2,3", no_control=True, offered=1)
        self.assertGreater(res["arms"]["2@1"]["mqtt_down_samples"], 0)

    def test_out_dir_has_sweep_json_request_logs_and_a_matching_sha256sums(self):
        host = self.start()
        res, d = self.sweep(host, repeats=1, sweep="2", no_control=True)
        names = sorted(os.listdir(d))
        self.assertIn("sweep.json", names)
        self.assertIn("SHA256SUMS", names)
        self.assertTrue(any(n.startswith("requests-2@6-r1") for n in names))
        want = hashlib.sha256(Path(d, "sweep.json").read_bytes()).hexdigest()
        self.assertEqual(Path(d, "SHA256SUMS").read_text().split()[0], want)
        line = Path(d, "requests-2@6-r1.ndjson").read_text(encoding="utf-8").splitlines()[0]
        self.assertEqual(set(json.loads(line)), {"t", "path", "status", "ms", "err"})

    def test_own_load_and_ui_probe_are_recorded(self):
        host = self.start()
        res, d = self.sweep(host, repeats=1, sweep="2", no_control=True, seconds=1.5)
        row = res["repeats_detail"][0]
        self.assertGreater(row["own_load"]["load_rps"], 5)
        self.assertGreaterEqual(row["own_load"]["total_rps"], row["own_load"]["load_rps"])
        self.assertGreaterEqual(row["ui_probe"]["count"], 1)
        self.assertEqual(row["ui_probe"]["failed"], 0)

    def test_cli_end_to_end(self):
        host = self.start()
        d = tempfile.mkdtemp()
        self.addCleanup(lambda: __import__("shutil").rmtree(d, ignore_errors=True))
        p = subprocess.run([sys.executable, SCRIPT, "--restcap", "--host", host, "--sweep", "2,5", "--offered", "5",
                            "--seconds", "1", "--repeats", "2", "--think-ms", "0", "--sample-interval", "0.15",
                            "--settle", "0", "--no-reset", "--no-ws", "--out-dir", d, "--ui-probe-interval", "0"],
                           capture_output=True, text=True, timeout=60)
        self.assertEqual(p.returncode, 0, p.stderr)
        out = json.loads(p.stdout)
        self.assertEqual(set(out), {"arms", "realistic_arms", "control", "decision"})
        self.assertIn("rule", out["decision"])


class Decision(unittest.TestCase):
    def test_regression_mode_ignores_the_503_criterion(self):
        arms = {2: dict(self.BASE), 6: dict(self.BASE, n=6, share_503=0.5)}
        self.assertFalse(sweep.evaluate_decision(arms)["per_n"][6]["pass"])
        d = sweep.evaluate_decision(arms, regression=True)
        self.assertTrue(d["per_n"][6]["pass"])
        self.assertIsNone(d["new_default"])

    BASE = dict(n=2, repeats_run=3, repeats_errored=0, errors=[], aborted=[], requests=1000, r503=500, share_503=0.5,
                other_failures=0, p95_ms_ok=80.0, p99_ms_ok=100.0, freeheap_min_sampled=40000, maxblk_min_sampled=40000, sample_share_min=1.0, crashlogs=0,
                hd_min_free_heap_after_min=30000, effective_cap_share_at_n=1.0, reboots=0, mqtt_down_samples=0,
                ws_disconnects=0, load_rps_mean=50.0, ui_probe_failed=0)

    def arms(self, **over):
        a = dict(self.BASE, n=6, share_503=0.30, p99_ms_ok=150.0, freeheap_min_sampled=35000)
        a.update(over)
        return {2: dict(self.BASE), 6: a}

    def dec(self, **over):
        return sweep.evaluate_decision(self.arms(**over))

    def test_a_good_arm_wins(self):
        d = self.dec()
        self.assertEqual(d["new_default"], 6)
        self.assertTrue(d["per_n"][6]["pass"])

    def test_each_criterion_can_fail_alone(self):
        cases = {
            "1_survival": dict(reboots=1),
            "1_survival ws": dict(ws_disconnects=1),
            "1_survival mqtt": dict(mqtt_down_samples=1),
            "1_survival abort": dict(aborted=["x"]),
            "2_heap floor": dict(hd_min_free_heap_after_min=8191),
            "2_heap rel": dict(freeheap_min_sampled=29999),
            "p99": dict(p99_ms_ok=200.1),
            "503": dict(share_503=0.351),
            "repeats": dict(repeats_run=2),
            "2_heap samples": dict(sample_share_min=0.79),
            "2_heap blk": dict(maxblk_min_sampled=29999),
            "1_survival crash": dict(crashlogs=1),
            "3_effective_cap": dict(effective_cap_share_at_n=0.94),
        }
        for name, over in cases.items():
            with self.subTest(name):
                d = self.dec(**over)
                self.assertFalse(d["per_n"][6]["pass"])
                self.assertEqual(d["new_default"], 2)
                key = name.split()[0]
                self.assertFalse(d["per_n"][6]["criteria"][key])
                others = [k for k, v in d["per_n"][6]["criteria"].items() if k != key and not v]
                self.assertEqual(others, [], "only the named criterion fails")

    def test_boundaries_pass_exactly(self):
        d = self.dec(share_503=0.35, p99_ms_ok=200.0, freeheap_min_sampled=30000, hd_min_free_heap_after_min=8192,
                     effective_cap_share_at_n=0.95, sample_share_min=0.8, maxblk_min_sampled=30000)
        self.assertTrue(d["per_n"][6]["pass"], d["per_n"][6])

    def test_a_base_without_any_503_shows_nothing_to_reduce(self):
        arms = self.arms()
        arms[2].update(share_503=0.0, r503=0)
        d = sweep.evaluate_decision(arms)
        self.assertFalse(d["per_n"][6]["criteria"]["503"])
        self.assertEqual(d["new_default"], 2)

    def test_the_largest_passing_n_is_the_new_default(self):
        arms = self.arms()
        arms[4] = dict(arms[6], n=4)
        arms[8] = dict(arms[6], n=8, reboots=2)
        d = sweep.evaluate_decision(arms)
        self.assertEqual(d["new_default"], 6)
        self.assertFalse(d["per_n"][8]["pass"])

    def test_no_base_arm_gives_no_decision(self):
        d = sweep.evaluate_decision({6: dict(self.BASE, n=6)})
        self.assertEqual(d["new_default"], 2)
        self.assertIn("no usable N=2 base arm", d["reason"])


class Helpers(unittest.TestCase):
    def test_percentile_is_nearest_rank(self):
        v = list(range(1, 101))
        self.assertEqual((sweep.percentile(v, 50), sweep.percentile(v, 95), sweep.percentile(v, 99)), (50, 95, 99))
        self.assertEqual(sweep.percentile([7], 99), 7)
        self.assertIsNone(sweep.percentile([], 50))

    def test_effective_cap_mirrors_the_firmware_tiers(self):
        for n in (2, 5, 8):
            self.assertEqual(sweep.effective_cap(n, 24000), n)
            self.assertEqual(sweep.effective_cap(n, 23999), min(n, 2))
            self.assertEqual(sweep.effective_cap(n, 16000), min(n, 2))
            self.assertEqual(sweep.effective_cap(n, 15999), 1)
        self.assertIsNone(sweep.effective_cap(4, None))

    def test_telnet_reset_sends_z_after_the_banner(self):
        got = []
        srv = socket.socket()
        srv.bind(("127.0.0.1", 0))
        srv.listen(1)

        def serve():
            c, _ = srv.accept()
            c.sendall(b"OTGW banner\r\n")
            got.append(c.recv(16))
            c.close()
        threading.Thread(target=serve, daemon=True).start()
        self.assertTrue(sweep.telnet_reset("127.0.0.1", srv.getsockname()[1], timeout=2.0))
        self.assertEqual(got, [b"z"])
        srv.close()
        self.assertFalse(sweep.telnet_reset("127.0.0.1", 1, timeout=0.5), "an unreachable telnet is reported, not raised")

    def test_ws_watcher_counts_frames_and_a_drop(self):
        class Conn:
            n = 0

            def settimeout(self, t):
                pass

            def recv(self):
                Conn.n += 1
                if Conn.n > 3:
                    raise RuntimeError("closed")
                return "frame"

            def close(self):
                pass

        fake = SimpleNamespace(create_connection=lambda url, timeout: Conn(), WebSocketTimeoutException=TimeoutError)
        with mock.patch.object(sweep, "websocket", fake):
            w = sweep.WsWatcher("h")
            w.start()
            time.sleep(0.3)
            out = w.stop()
        self.assertEqual(out, {"connected": True, "frames": 3, "dropped": True})

    def test_the_old_phase1_sweep_is_still_there(self):
        self.assertEqual(sweep.SWEEP_N, [1, 2, 3, 4, 6, 8])
        self.assertTrue(callable(sweep.run_arm) and callable(sweep.hwm_snapshot))
        p = subprocess.run([sys.executable, SCRIPT, "--help"], capture_output=True, text=True, timeout=30)
        self.assertIn("--scenario-seconds", p.stdout)
        self.assertIn("--restcap", p.stdout)


if __name__ == "__main__":
    unittest.main(verbosity=2)
