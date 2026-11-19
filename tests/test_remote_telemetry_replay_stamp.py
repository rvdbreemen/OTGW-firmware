#!/usr/bin/env python3
"""TASK-2901: a replayed frame is stamped so the decoded gaps equal the replay file's gaps to the millisecond.

Compiles the real remoteTelemetryOtFrame() of RemoteTelemetryDevice.h (from the replay-anchor comment, or from the
function itself on older code, to the end of the function) with stubs: millis() and the wall clock tick together, but
the wall clock runs 0.6 ms out of phase with millis() (the case on .63, where two separate roundings made half the
frames 1 ms early). Each frame is processed 0-40 ms after its ideal time. Every stamped gap must equal the ideal gap.
Red on bc6c4ccf4 (gaps off by 1 ms), green on the single-anchor fix. RTEL_SRC=<dir> tests another copy.
Run: python tests/test_remote_telemetry_replay_stamp.py
"""
import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
from test_sat_pwm_override import compile_cpp  # noqa: E402

SRC = Path(os.environ.get("RTEL_SRC", HERE.parent / "src" / "OTGW-firmware"))

PRE = r"""
#include <cstdint>
#include <cstdio>
static uint64_t g_us = 0;                       // wall clock, us
static const uint64_t PHASE_US = 600;           // wall clock ahead of millis() by 0.6 ms
static uint32_t millis() { return (uint32_t)(g_us / 1000); }
static int64_t remoteTelemetryNowUs() { return (int64_t)(g_us + PHASE_US + 1763600000000000ULL); }
static volatile bool remoteTelemetryActive = true;
static uint32_t otFrameIdealMs = 0;
static uint32_t remoteTelemetryFrameSeq = 0;
static int64_t g_stamp = 0;
struct Rec { bool otFrame(int64_t t, char, uint32_t, uint32_t) { g_stamp = t; return true; } } remoteTelemetry;
template <class F> static void remoteTelemetryAppend(int64_t, F f) { f(); }
"""

MAIN = r"""
int main() {
  uint32_t ideal = 5000;
  uint32_t r = 12345;
  for (int k = 0; k < 4000; k++) {
    r ^= r << 13; r ^= r >> 17; r ^= r << 5;
    ideal += 20 + r % 980;                        // the replay's gaps: whole ms
    g_us = (uint64_t)ideal * 1000 + (r % 40000);  // processed 0-40 ms late, at any us
    otFrameIdealMs = ideal;
    remoteTelemetryOtFrame('B', 0x40000000u);
    printf("%u %lld\n", ideal, (long long)g_stamp);
  }
  return 0;
}
"""


def extract(dev):
    end_marker = "void remoteTelemetryOtFrame(char letter, uint32_t frame) {"
    f = dev.index(end_marker)
    a = dev.find("// TASK-2901: one epoch <-> millis() anchor for replayed frames.")
    if a < 0 or a > f:
        a = f
    else:
        # the anchor block sits above remoteTelemetryEvSession(); keep only the declarations
        decl_end = dev.index("static void remoteTelemetryEvSession()", a)
        anchor = dev[a:decl_end]
        a = None
    body_end = dev.index("\n}\n", f) + 3
    fn = dev[f:body_end]
    return (anchor if a is None else "") + fn


class ReplayStamp(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        dev = (SRC / "RemoteTelemetryDevice.h").read_text(encoding="utf-8")
        cls.tmp = tempfile.TemporaryDirectory()
        cpp = Path(cls.tmp.name) / "s.cpp"
        cpp.write_text(PRE + extract(dev) + MAIN, encoding="utf-8")
        exe = compile_cpp(cpp, Path(cls.tmp.name) / ("s.exe" if os.name == "nt" else "s"))
        if exe is None:
            raise unittest.SkipTest("no host compiler")
        out = subprocess.run([str(exe)], capture_output=True, text=True, check=True).stdout.split("\n")
        cls.rows = [tuple(map(int, l.split())) for l in out if l.strip()]

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def test_gaps_exact(self):
        bad = [(a, b) for (ia, sa), (ib, sb) in zip(self.rows, self.rows[1:])
               if (sb - sa) != (ib - ia) for a, b in [((ia, sa), (ib, sb))]]
        self.assertEqual(len(self.rows), 4000)
        self.assertEqual(len(bad), 0, f"{len(bad)} of 3999 gaps differ from the replay's gaps, e.g. {bad[:3]}")


if __name__ == "__main__":
    unittest.main(verbosity=2)
