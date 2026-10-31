#!/usr/bin/env python3
"""TASK-1521: the SAT instrumentation lines and scripts/sat_log_to_csv.py fit together.

1. The firmware source logs the record types the parser reads (SATTICK, SATBOOT, SATMODE, SATTC, SATFOLLOW, SATDHW),
   with the SATTICK keys a simulation needs. OTGW_SAT_INO points at another copy for an old-vs-fix run: the
   pre-TASK-1521 SATcontrol.ino has none of them, so these cases fail on it.
2. The format string of SATTICK, filled with sample values the way printf does, parses back into the same keys and
   values (the C format is taken from the source, not copied into this test).
3. A transcript fragment turns into one CSV per record type.

Not reachable here: the real telnet output (bench capture, task evidence).

Run: python tests/test_sat_log_to_csv.py
"""
import csv
import os
import re
import sys
import tempfile
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
SRC = REPO / "src" / "OTGW-firmware"
SAT_INO = Path(os.environ.get("OTGW_SAT_INO", SRC / "SATcontrol.ino"))
sys.path.insert(0, str(REPO / "scripts"))
import sat_log_to_csv as p  # noqa: E402

TICK_KEYS = ["room", "raw", "rv", "target", "out", "osrc", "curve", "p", "i", "d", "praw", "pid", "cs", "mm", "ch",
             "mode", "pwm", "duty", "flame", "flow", "ret", "dhw"]


def tick_format(src):
    m = re.search(r'SATDebugTf\(PSTR\("(SATTICK room=[^"]*)\\r\\n"\)', src)
    return m.group(1) if m else None


class SourceHasRecords(unittest.TestCase):
    def setUp(self):
        self.src = SAT_INO.read_text(encoding="utf-8")

    def test_tick_line_with_all_keys(self):
        fmt = tick_format(self.src)
        self.assertIsNotNone(fmt, "no SATTICK line in SATcontrol.ino")
        self.assertEqual(re.findall(r"(\w+)=", fmt), TICK_KEYS)

    def test_event_records_present(self):
        for kind in ("SATBOOT phase=decide", "SATBOOT phase=boiler", "SATBOOT phase=outside", "SATBOOT phase=takeover",
                     "dhw=1", "SATMODE from=", "SATTC push=",
                     "SATTC refused=", "SATFOLLOW recorded=", "SATDHW start", "SATDHW end"):
            self.assertIn(kind, self.src, kind)

    def test_trace_level_is_a_separate_default_off_toggle(self):
        dbg = (SRC / "debugStuff.h").read_text(encoding="utf-8")
        self.assertRegex(dbg, r"bool\s+bSATTrace\s*=\s*false;")
        self.assertRegex(dbg, r"bool\s+bSAT\s*=\s*true;")
        self.assertIn("case '8':", (SRC / "handleDebug.ino").read_text(encoding="utf-8"))
        self.assertIn("SATTraceTln(F(\"SAT: publishing MQTT state", self.src)


class Parser(unittest.TestCase):
    def test_tick_format_round_trip(self):
        fmt = tick_format(SAT_INO.read_text(encoding="utf-8"))
        if fmt is None:
            self.fail("no SATTICK line in SATcontrol.ino")
        vals = [19.25, 19.31, 1, 20.0, 7.5, "W", 29.31, 0.5, 1.25, -0.03, 31.03, 31.06, 37.8, 100, 1, 2, 1, 0.54, 1,
                38.8, 30.1]
        line = "10:04:38.127074 (  55700| 31732) satControlLo(6192): " + (fmt % tuple(vals))
        rows = p.parse([line])["SATTICK"]
        self.assertEqual(len(rows), 1)
        self.assertEqual(list(rows[0])[1:], TICK_KEYS)
        self.assertEqual(rows[0]["time"], "10:04:38.127074")
        self.assertAlmostEqual(float(rows[0]["cs"]), 37.8)
        self.assertEqual(rows[0]["mm"], "100")

    def test_transcript_to_csv_files(self):
        lines = [
            "noise line",
            "11:11:14.817421 (  91568| 47092) satRestartSt(3434): SATBOOT phase=decide ms=12000 valid=0 pidload=0 ntp=1 tc=1",
            "11:11:14.821000 (  91568| 47092) satEnqueueIf(652): SATTC push=20.0",
            "11:11:20.000000 (  91568| 47092) satControlLo(5880): SATDHW start",
            "11:11:44.856188 (  76940| 31732) satControlLo(6192): SATTICK room=20.00 target=20.0 out=7.2 curve=21.50 "
            "p=0.00 i=0.00 d=0.00 pid=21.50 cs=21.5 mm=100 ch=1 mode=1 pwm=0 duty=0.00 flame=0 flow=24.9 ret=25.0",
        ]
        with tempfile.TemporaryDirectory() as d:
            t = Path(d) / "cap.txt"
            t.write_text("\n".join(lines) + "\n", encoding="utf-8")
            rc = p.main([str(t), "--out-dir", d])
            self.assertEqual(rc, 0)
            with open(Path(d) / "cap-sattick.csv", newline="") as f:
                rows = list(csv.DictReader(f))
            self.assertEqual(rows[0]["cs"], "21.5")
            with open(Path(d) / "cap-satdhw.csv", newline="") as f:
                self.assertEqual(list(csv.DictReader(f))[0]["event"], "start")
            with open(Path(d) / "cap-satboot.csv", newline="") as f:
                self.assertEqual(list(csv.DictReader(f))[0]["ms"], "12000")


if __name__ == "__main__":
    unittest.main()
