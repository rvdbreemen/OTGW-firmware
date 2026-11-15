#!/usr/bin/env python3
"""
TASK-1964: the capture tool collects the device graph history and a diagnostic recording.

scripts/otgw_capture.py fetches GET /api/v2/graph/history (ADR-193) and keeps it as JSON and as
the web UI CSV export, and downloads a diagnostic recording (ADR-213) the device holds, with GETs
only (a stop would make the PSRAM ring unreadable, TASK-1960).

Cases:
  a  converter: a graph/history response in the shape graphHistEmitJson() writes becomes the web UI
     CSV: header, epoch_ms per slot (start of the slot, snapped to 5 s), temperatures / scale, mod,
     0/1 flags, empty cells for null, pure gap slots dropped, unusable responses refused
  b  the same CSV as the REAL v2.js: mergeGraphHistory() and the GSER column order are cut out of
     data/v2.js and run with node; its records, joined the way exportCsv() joins them, must equal the
     converter's text byte for byte (skipped when node is missing)
  c  fetch_graph_history() against a fake device: 24 h kept; 7 d kept only when its interval is
     coarser (an old firmware that ignores ?hours=168 is not stored twice); 404 and junk are one line
  d  fetch_diag_recording() against a fake device: nothing fetched without a recording, a download
     with one, since= for the next pull; no POST, PUT or start/stop path is ever requested
  e  build_upload_zip() puts the graph-history-*, diag-recording-* and device-state-* files into the upload zip
  f  sweep_device() against a fake device: every listed GET once, GET only, secret keys redacted at any depth,
     the hourly pass only the changing routes, never an excluded route
  g  route gate: every top-level REST resource of restAPI.ino is covered by the sweep list or by the excluded
     list with a reason, so a new GET route cannot slip past the capture unnoticed

Red on the script before TASK-1964 (no converter, no fetchers); green on the fix.
Run: python tests/test_capture_device_data.py
"""

import json
import re
import shutil
import subprocess
import sys
import tempfile
import threading
import time
import unittest
import zipfile
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "scripts"))
import otgw_capture as oc  # noqa: E402

NOW_MS = 1763200000000 + 1234   # an arbitrary host time with a non-zero remainder


def response(n=6, iv=120, age=37000):
    """A response in the exact shape graphHistEmitJson() writes (graphHistoryStuff.h)."""
    return {
        "interval_s": iv, "newest_age_ms": age, "count": n, "scale": 10,
        "flow":    [452, None, None, 455, 460, -5][:n],
        "ret":     [301, None, None, 302, 303, 0][:n],
        "chSet":   [500, None, None, 500, 500, 100][:n],
        "roomSet": [205, None, None, 205, 205, 205][:n],
        "room":    [198, None, None, None, 201, 199][:n],
        "outside": [73, None, None, 72, 71, -12][:n],
        "mod":     [34, None, None, 0, 100, None][:n],
        "flame":   [1, 0, 0, 0, 1, 0][:n],
        "dhw_on":  [0, 0, 1, 0, 0, 0][:n],
        "ch":      [1, 0, 0, 1, 1, 0][:n],
    }


class ConverterCase(unittest.TestCase):
    def test_header_and_rows(self):
        text = oc.graph_history_csv(response(), NOW_MS)
        lines = text.split("\n")
        self.assertEqual(lines[0], "epoch_ms,flow,ret,chSet,roomSet,room,outside,mod,flame,dhw_on,ch")
        self.assertFalse(text.endswith("\n"))
        # slot 1 has no value and no flag: dropped; slot 2 has only dhw_on: kept
        self.assertEqual(len(lines), 1 + 5)
        t0 = NOW_MS - 37000 - 6 * 120000
        t0 -= t0 % 5000
        self.assertEqual(lines[1], f"{t0},45.2,30.1,50,20.5,19.8,7.3,34,1,0,1")
        t2 = NOW_MS - 37000 - 4 * 120000
        t2 -= t2 % 5000
        self.assertEqual(lines[2], f"{t2},,,,,,,,0,1,0")
        self.assertTrue(lines[3].endswith(",45.5,30.2,50,20.5,,7.2,0,0,0,1"))   # room null -> empty cell
        self.assertTrue(lines[5].endswith(",-0.5,0,10,20.5,19.9,-1.2,,0,0,0"))

    def test_slot_times_step_by_the_interval(self):
        rows = oc.graph_history_rows(response(iv=30), NOW_MS)
        ts = [t for t, _ in rows]
        self.assertTrue(all(t % 5000 == 0 for t in ts))
        self.assertEqual(ts[-1] - ts[-2], 30000)

    def test_unusable_responses_give_none(self):
        bad = [None, [], {}, dict(response(), count=0), dict(response(), scale=0),
               dict(response(), interval_s="120"), dict(response(), newest_age_ms=-1),
               dict(response(), flow=[1, 2])]
        for j in bad:
            self.assertIsNone(oc.graph_history_csv(j, NOW_MS), j)


NODE_SCRIPT = r"""
const fs = require('fs');
const src = fs.readFileSync(process.argv[2], 'utf8');
function extract(name) {
  const start = src.indexOf('function ' + name + '(');
  if (start < 0) throw new Error(name + ' not found');
  let i = src.indexOf('{', start), depth = 0;
  for (; i < src.length; i++) {
    const c = src[i];
    if (c === "'" || c === '"' || c === '`') { for (i++; i < src.length && src[i] !== c; i++) if (src[i] === '\\') i++; }
    else if (c === '/' && src[i + 1] === '/') { i = src.indexOf('\n', i); }
    else if (c === '/' && src[i + 1] === '*') { i = src.indexOf('*/', i) + 1; }
    else if (c === '{') depth++;
    else if (c === '}' && --depth === 0) return src.slice(start, i + 1);
  }
  throw new Error('unbalanced ' + name);
}
// GSER is a literal array of strings in our own shipped v2.js; eval of that one literal is the simplest exact read.
const gser = src.match(/var GSER = (\[[\s\S]*?\]\]);/);
const GSER = eval(gser[1]);
const merge = new Function('GSLOT', 'GHIST_MAX', extract('mergeGraphHistory') + '\nreturn mergeGraphHistory;')(5000, 86400000 / 5000);
const j = JSON.parse(fs.readFileSync(process.argv[3], 'utf8'));
const arr = merge([], j, Number(process.argv[4]));
const keys = GSER.map(function (x) { return x[0]; });
// exportCsv(): header and rows exactly as v2.js joins them (no Dallas keys: the records have d: null)
const lines = ['epoch_ms,' + keys.map(function (k) { return k.replace(':', '_'); }).join(',')];
arr.forEach(function (s) { lines.push(s.t + ',' + keys.map(function (k) { var x = s[k]; return x === null || x === undefined ? '' : x; }).join(',')); });
process.stdout.write(lines.join('\n'));
"""


@unittest.skipIf(shutil.which("node") is None, "node not found")
class SameAsWebUiCase(unittest.TestCase):
    def test_byte_identical_to_v2js(self):
        v2 = ROOT / "src" / "OTGW-firmware" / "data" / "v2.js"
        src = v2.read_text(encoding="utf-8")
        self.assertIn("lines.join('\\n')", src)                      # exportCsv still joins with LF
        self.assertIn("return x === null || x === undefined ? '' : x", src)   # and prints null as empty
        with tempfile.TemporaryDirectory() as tmp:
            js = Path(tmp) / "csv.js"
            js.write_text(NODE_SCRIPT, encoding="utf-8")
            for j in (response(), response(iv=30, age=0), response(n=3, age=119999)):
                body = Path(tmp) / "h.json"
                body.write_text(json.dumps(j), encoding="utf-8")
                r = subprocess.run(["node", str(js), str(v2), str(body), str(NOW_MS)],
                                   capture_output=True, text=True, timeout=60)
                self.assertEqual(r.returncode, 0, r.stderr)
                self.assertEqual(oc.graph_history_csv(j, NOW_MS), r.stdout)


class FakeDevice:
    """A tiny HTTP server that answers fixed bodies per path and records every request."""

    def __init__(self, routes):
        self.routes = routes
        self.seen = []
        dev = self

        class H(BaseHTTPRequestHandler):
            def log_message(self, *a):
                pass

            def answer(self):
                dev.seen.append((self.command, self.path))
                status, body = dev.routes.get(self.path, (404, b"{}"))
                self.send_response(status)
                self.send_header("Content-Length", str(len(body)))
                self.end_headers()
                self.wfile.write(body)

            do_GET = answer
            do_POST = answer
            do_PUT = answer

        self.httpd = ThreadingHTTPServer(("127.0.0.1", 0), H)
        threading.Thread(target=self.httpd.serve_forever, daemon=True).start()
        self.base = "http://127.0.0.1:%d" % self.httpd.server_address[1]

    def close(self):
        self.httpd.shutdown()
        self.httpd.server_close()


class DeviceTestBase(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        run = Path(self.tmp.name) / "20261115-120000"
        run.mkdir()
        self.cap = oc.Capture(run, echo=False)
        self.addCleanup(self.cap.store.close)

    def device(self, routes):
        d = FakeDevice(routes)
        self.addCleanup(d.close)
        return d

    def files(self, pattern):
        return sorted(p.name for p in self.cap.run_path.glob(pattern))


class FetchCase(DeviceTestBase):
    def test_old_firmware_keeps_only_24h(self):
        body = json.dumps(response()).encode()
        d = self.device({"/api/v2/graph/history": (200, body), "/api/v2/graph/history?hours=168": (200, body)})
        lines = oc.fetch_graph_history(self.cap, d.base, "start")
        self.assertEqual(len(self.files("graph-history-24h-start-*.csv")), 1)
        self.assertEqual(len(self.files("graph-history-24h-start-*.json")), 1)
        self.assertEqual(self.files("graph-history-7d-*"), [])
        self.assertIn("not supported", lines[1])
        csv_file = self.cap.run_path / self.files("graph-history-24h-start-*.csv")[0]
        self.assertTrue(csv_file.read_text(encoding="utf-8").startswith("epoch_ms,flow,"))

    def test_new_firmware_keeps_both(self):
        d = self.device({"/api/v2/graph/history": (200, json.dumps(response()).encode()),
                         "/api/v2/graph/history?hours=168": (200, json.dumps(response(iv=600)).encode())})
        oc.fetch_graph_history(self.cap, d.base, "stop")
        self.assertEqual(len(self.files("graph-history-24h-stop-*.csv")), 1)
        self.assertEqual(len(self.files("graph-history-7d-stop-*.csv")), 1)

    def test_404_and_junk_are_one_line_each(self):
        d = self.device({"/api/v2/graph/history": (200, b"not json")})
        lines = oc.fetch_graph_history(self.cap, d.base, "start")
        self.assertEqual(len(lines), 2)
        self.assertIn("not JSON", lines[0])
        self.assertIn("HTTP 404", lines[1])
        self.assertEqual(self.files("graph-history-*"), [])

    def test_no_recording_fetches_nothing(self):
        st = {"recording": {"state": "stopped", "active": False, "has_data": False, "newest_seq": 0}}
        d = self.device({"/api/v2/diag/recording/status": (200, json.dumps(st).encode())})
        line, _ = oc.fetch_diag_recording(self.cap, d.base, "start")
        self.assertIn("none on the device", line)
        self.assertEqual(d.seen, [("GET", "/api/v2/diag/recording/status")])
        self.assertEqual(self.files("diag-recording-*"), [])

    def test_recording_is_downloaded_with_gets_only(self):
        st = {"recording": {"state": "recording", "active": True, "has_data": True, "oldest_seq": 3, "newest_seq": 9}}
        st2 = dict(st, recording=dict(st["recording"], newest_seq=12))
        d = self.device({"/api/v2/diag/recording/status": (200, json.dumps(st).encode()),
                         "/api/v2/diag/recording": (200, b"OTGWDIAG" + bytes(100)),
                         "/api/v2/diag/recording?since=9": (200, b"OTGWDIAG" + bytes(40))})
        line, newest = oc.fetch_diag_recording(self.cap, d.base, "start")
        self.assertEqual(newest, 9)
        self.assertIn("108 bytes", line)
        d.routes["/api/v2/diag/recording/status"] = (200, json.dumps(st2).encode())
        line, newest = oc.fetch_diag_recording(self.cap, d.base, "stop", since=9)
        self.assertEqual(newest, 12)
        self.assertIn("since 9", line)
        self.assertEqual(len(self.files("diag-recording-*.bin")), 2)
        self.assertTrue(all(m == "GET" for m, _ in d.seen), d.seen)
        self.assertFalse(any(p.endswith(("/start", "/stop")) for _, p in d.seen), d.seen)

    def test_no_new_blocks_skips_the_download(self):
        st = {"recording": {"state": "recording", "active": True, "has_data": True, "newest_seq": 9}}
        d = self.device({"/api/v2/diag/recording/status": (200, json.dumps(st).encode())})
        line, _ = oc.fetch_diag_recording(self.cap, d.base, "hourly", since=9)
        self.assertIn("no new blocks", line)
        self.assertEqual(len(d.seen), 1)

    def test_upload_zip_holds_the_files(self):
        for name in ("graph-history-24h-stop-1.csv", "graph-history-24h-stop-1.json", "diag-recording-stop-1.bin"):
            self.cap.path(name).write_bytes(b"x")
        transcript = self.cap.path("transcript-x.txt")
        transcript.write_text("t", encoding="utf-8")
        target = oc.build_upload_zip(self.cap, transcript, False)
        with zipfile.ZipFile(target) as z:
            names = set(z.namelist())
        self.assertTrue({"graph-history-24h-stop-1.csv", "graph-history-24h-stop-1.json",
                         "diag-recording-stop-1.bin"} <= names, names)


class SweepCase(DeviceTestBase):
    SETTINGS = {"settings": {"mqttpasswd": {"value": "password=8", "type": "p"},
                             "webhookurlon": {"value": "http://x/?token=abc", "type": "s"},
                             "hostname": {"value": "otgw", "type": "s"}}}
    WEATHER = {"weather": {"latitude": 52.1, "longitude": 5.1, "temp": 7.0}}

    def routes(self):
        r = {p: (200, b'{"ok":true}') for p, _ in oc.SWEEP_ENDPOINTS}
        r["/api/v2/settings"] = (200, json.dumps(self.SETTINGS).encode())
        r["/api/v2/sat/weather"] = (200, json.dumps(self.WEATHER).encode())
        r["/reboot_log.txt"] = (200, b"boot 1 password=hunter2\n")
        return r

    def test_full_sweep_is_get_only_and_redacted(self):
        d = self.device(self.routes())
        line = oc.sweep_device(self.cap, d.base, "start")
        expected = [p for p, _ in oc.SWEEP_ENDPOINTS] + list(oc.SWEEP_FILES)
        self.assertEqual([p for _, p in d.seen], expected)
        self.assertTrue(all(m == "GET" for m, _ in d.seen))
        self.assertFalse(set(expected) & set(oc.SWEEP_EXCLUDED))
        text = (self.cap.run_path / self.files("device-state-start-*.txt")[0]).read_text(encoding="utf-8")
        for secret in ("password=8", "token=abc", "52.1", "5.1,", "hunter2"):
            self.assertNotIn(secret, text)
        self.assertIn('"hostname"', text)
        self.assertIn("[REDACTED]", text)
        self.assertIn("404", line)   # the files the fake device does not serve

    def test_hourly_sweep_reads_only_the_changing_routes(self):
        d = self.device(self.routes())
        oc.sweep_device(self.cap, d.base, "hourly", hourly_only=True)
        self.assertEqual([p for _, p in d.seen], [p for p, changes in oc.SWEEP_ENDPOINTS if changes])

    def test_redact_json_walks_lists_and_nested_objects(self):
        out = oc.redact_json({"a": [{"bindkey": "k", "x": 1}], "b": {"httppasswd": {"value": "y"}}})
        self.assertEqual(out, {"a": [{"bindkey": "[REDACTED]", "x": 1}], "b": {"httppasswd": "[REDACTED]"}})


class DeviceDataThreadCase(DeviceTestBase):
    def test_start_round_then_hourly_rounds_with_since(self):
        st = {"recording": {"state": "recording", "active": True, "has_data": True, "oldest_seq": 1, "newest_seq": 5}}
        routes = {p: (200, b"{}") for p, _ in oc.SWEEP_ENDPOINTS}
        routes.update({"/api/v2/graph/history": (200, json.dumps(response()).encode()),
                       "/api/v2/diag/recording/status": (200, json.dumps(st).encode()),
                       "/api/v2/diag/recording": (200, b"OTGWDIAG" + bytes(10))})
        d = self.device(routes)
        t = oc.DeviceDataCapture(self.cap, d.base, hourly=True, interval=0.3)
        t.start()
        deadline = time.monotonic() + 20
        while time.monotonic() < deadline and len(self.files("device-state-hourly-*")) < 2:
            time.sleep(0.1)
        self.cap.request_stop("test")
        t.join(10)
        self.assertFalse(t.is_alive())
        self.assertEqual(len(self.files("device-state-start-*")), 1)
        self.assertGreaterEqual(len(self.files("device-state-hourly-*")), 2)
        self.assertEqual(len(self.files("graph-history-24h-start-*.csv")), 1)
        self.assertEqual(t.diag_since, 5)
        self.assertEqual(len(self.files("diag-recording-start-*.bin")), 1)
        self.assertEqual(self.files("diag-recording-hourly-*"), [])   # no new blocks after 5
        self.assertNotIn(("GET", "/api/v2/pic/settings"),
                         [s for s in d.seen[len(oc.SWEEP_ENDPOINTS) + len(oc.SWEEP_FILES):]])   # start round only

    def test_short_run_does_only_the_start_round(self):
        d = self.device({})
        t = oc.DeviceDataCapture(self.cap, d.base, hourly=False)
        t.start()
        t.join(30)
        self.assertFalse(t.is_alive())
        self.assertEqual(len(self.files("device-state-start-*")), 1)
        self.assertEqual(self.files("device-state-hourly-*"), [])


class RouteGateCase(unittest.TestCase):
    def test_every_rest_resource_is_listed(self):
        src = (ROOT / "src" / "OTGW-firmware" / "restAPI.ino").read_text(encoding="utf-8")
        names = re.findall(r'static const char kRoute\w+\[\]\s+PROGMEM = "([^"]+)";', src)
        self.assertGreater(len(names), 10)
        listed = [p for p, _ in oc.SWEEP_ENDPOINTS] + list(oc.SWEEP_EXCLUDED)
        missing = [n for n in names if not any(p == "/api/v2/" + n or p.startswith("/api/v2/" + n + "/")
                                               or p.startswith("/api/v2/" + n + "?") for p in listed)]
        self.assertEqual(missing, [], "add these REST resources to SWEEP_ENDPOINTS or SWEEP_EXCLUDED "
                                      "(scripts/otgw_capture.py) and to the TASK-1964 list")

    def test_every_exclusion_has_a_reason(self):
        self.assertTrue(all(isinstance(v, str) and len(v) > 10 for v in oc.SWEEP_EXCLUDED.values()))


if __name__ == "__main__":
    unittest.main(verbosity=2)
