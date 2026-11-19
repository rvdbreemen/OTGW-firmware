#!/usr/bin/env python3
"""TASK-2901 (Robert 2026-11-19): frame sequence numbers, session/date markers, day files, 3 ms, byte-identical extract.

Builds records the way the device does (pack_mix sections + the SEC_FSEQ and events sections) and runs the real
tools/remote-telemetry/decode_all.decode_records + write_days and compare_replay on them:
- a record whose first/last sequence numbers leave a hole -> '# frame gap' lines with the right counts;
- the session marker (EV 42) and the date marker (EV 43) appear as '#' lines in otframes.log;
- --days cuts one file per local calendar day, 00:00:00-24:00:00, across midnight;
- compare_replay fails a pass whose gaps are 4 ms off (limit 3 ms) and extracts a byte-identical copy of the input.
Red before the change (no SEC_FSEQ / write_days / --max-gap-ms / --extract). Run: python tests/test_remote_telemetry_seq_marker_days.py
"""
import base64
import datetime
import hashlib
import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from zoneinfo import ZoneInfo

TOOLS = Path(__file__).resolve().parent.parent / "tools" / "remote-telemetry"
sys.path.insert(0, str(TOOLS))
import otrec  # noqa: E402
import decode_all  # noqa: E402

TZ = ZoneInfo("Europe/Amsterdam")


def epoch_ms(y, mo, d, h, mi, s, ms=0):
    return int(datetime.datetime(y, mo, d, h, mi, s, ms * 1000, tzinfo=TZ).timestamp() * 1000)


def section(typ, data):
    return bytes([typ]) + otrec._leb(len(data)) + data


def record(seq, t0, frames, fseq=None, events=()):
    """frames: [(t_ms, letter, frame)]; events: [(t_ms, type, values)]."""
    payload = otrec.pack_mix(t0, ot_frames=frames)
    if fseq:
        payload += section(otrec.SEC_FSEQ, otrec._leb(fseq[0]) + otrec._leb(fseq[1]))
    if events:
        ev, prev = b"", t0
        for t, typ, vals in events:
            ev += otrec.evt_encode_item(t, prev, typ, vals)
            prev = t
        payload += section(otrec.SEC_EVT, ev)
    return {"seq": seq, "t0": t0, "fmt": otrec.FMT_MIX_V1, "payload": base64.b64encode(payload).decode(), "build_id": None}


class SeqMarkerDays(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.out = self.tmp.name

    def tearDown(self):
        self.tmp.cleanup()

    def decode(self, recs):
        c = decode_all.decode_records(recs, None, TZ, self.out)
        return c, open(os.path.join(self.out, "otframes.log"), encoding="utf-8").read()

    def test_frame_gap_between_and_inside_records(self):
        t0 = epoch_ms(2026, 11, 19, 21, 0, 0)
        f = lambda k: (t0 + 100 * k, "T", 0x00000300 + k)
        recs = [record(1, t0, [f(0), f(1), f(2)], fseq=(1, 3), events=[(t0, 42, [t0, 1, "fw", "otgw-x"])]),
                record(2, t0 + 1000, [f(10), f(11)], fseq=(6, 7)),             # 4, 5 missing before
                record(3, t0 + 2000, [f(20), f(21)], fseq=(8, 10))]            # 3 numbers, 2 frames: 1 inside
        c, log = self.decode(recs)
        self.assertIn("# frame gap: 2 frames (seq 4..5) missing before record 2", log)
        self.assertIn("# frame gap: 1 frames missing inside record 3 (seq 8..10)", log)
        self.assertEqual(c.get("frames_missing"), 3, log)

    def test_markers_and_days_across_midnight(self):
        t0 = epoch_ms(2026, 11, 19, 23, 59, 59, 900)
        mid = epoch_ms(2026, 11, 20, 0, 0, 0)
        recs = [record(1, t0, [(t0, "T", 0x00000300), (t0 + 50, "B", 0xC000030A)], fseq=(1, 2),
                       events=[(t0, 42, [t0, 1, "2.0.0-poc.24", "otgw-ac276ece45d8"]), (t0, 43, [epoch_ms(2026, 11, 19, 0, 0, 0), "2026-11-19"])]),
                record(2, mid + 5, [(mid + 5, "T", 0x00000300), (mid + 55, "B", 0xC000030A)], fseq=(3, 4),
                       events=[(mid + 1, 43, [mid, "2026-11-20"])])]
        c, log = self.decode(recs)
        self.assertIn("# session start 2026-11-19T23:59:59.900+01:00 firmware 2.0.0-poc.24 uid otgw-ac276ece45d8", log)
        self.assertIn("# date 2026-11-20", log)
        self.assertNotIn("frame gap", log)
        names, n = decode_all.write_days(self.out)
        self.assertEqual(names, ["2026-11-19", "2026-11-20"])
        d20 = open(os.path.join(self.out, "days", "2026-11-20.otframes.log"), encoding="utf-8").read().splitlines()
        self.assertIn("# date 2026-11-20", d20)
        self.assertTrue(d20[-2].split("\t")[1].startswith("00:00:00.005"), d20)


class CompareReplay(unittest.TestCase):
    def run_cmp(self, inp, got, *extra):
        return subprocess.run([sys.executable, "-I", str(TOOLS / "compare_replay.py"), inp, got, *extra],
                              capture_output=True, text=True)

    def test_3ms_limit_and_byte_identical_extract(self):
        with tempfile.TemporaryDirectory() as d:
            inp = os.path.join(d, "in.log")
            lines = ["00:00:%06.3f\t%s\t%08X" % (0.375 * k, "TB"[k % 2], 0x00000300 + k) for k in range(40)]
            Path(inp).write_bytes(("\n".join(lines) + "\n").encode())   # LF, like the real input files
            base = datetime.datetime(2026, 11, 19, 21, 36, 57, tzinfo=TZ)
            def download(jitter_ms):
                rows = []
                for k, l in enumerate(lines):
                    t, letter, fr = l.split("\t")
                    ts = base + datetime.timedelta(seconds=0.375 * k, milliseconds=jitter_ms if k == 20 else 0)
                    rows.append(f"{ts.isoformat(timespec='milliseconds')}\t{ts.strftime('%H:%M:%S.%f')}\t{letter}\t{fr}")
                p = os.path.join(d, f"got{jitter_ms}.log")
                Path(p).write_bytes(("# header\n" + "\n".join(rows) + "\n").encode())
                return p
            r = self.run_cmp(inp, download(4))
            self.assertEqual(r.returncode, 1, r.stdout)
            self.assertIn("OVER 3 ms", r.stdout)
            ext = os.path.join(d, "extract.log")
            r = self.run_cmp(inp, download(0), "--extract", ext)
            self.assertEqual(r.returncode, 0, r.stdout)
            self.assertEqual(hashlib.sha256(open(ext, "rb").read()).hexdigest(),
                             hashlib.sha256(open(inp, "rb").read()).hexdigest(), open(ext).read()[:300])


if __name__ == "__main__":
    unittest.main(verbosity=2)
