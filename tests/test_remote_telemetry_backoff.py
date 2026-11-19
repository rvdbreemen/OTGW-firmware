#!/usr/bin/env python3
"""TASK-2901 (Robert 2026-11-19): retry after about a minute, then exponential, spread at random; cap 15 min while
capturing, 60 min while paused.

Compiles the real rtel::backoffMs() of src/OTGW-firmware/RemoteTelemetryPolicy.h (only its backoff part: the rest of
the header uses GCC builtins) and checks the schedule over many random values: step 0 waits 30-60 s, each step doubles
the base, the default cap is 15 min (7.5-15 min at the cap), the pause cap is 60 min (30-60 min at the cap), and the
waits are spread so gateways that lost the backend together do not come back together.
Red on c2623c057 (first wait 5 min, cap 24 h, +0-20 % only, no cap parameter). Run: python tests/test_remote_telemetry_backoff.py
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

POLICY = (HERE.parent / "src" / "OTGW-firmware" / "RemoteTelemetryPolicy.h").read_text(encoding="utf-8")
MAIN = r"""
#include <cstdio>
int main() {
  uint32_t r = 2463534242u;
  for (int step = 0; step < 12; step++)
    for (int k = 0; k < 2000; k++) {
      r ^= r << 13; r ^= r >> 17; r ^= r << 5;
      printf("C %d %u\n", step, rtel::backoffMs((uint8_t)step, r));
      printf("P %d %u\n", step, rtel::backoffMs((uint8_t)step, r, rtel::BACKOFF_PAUSE_MAX_MS));
    }
  return 0;
}
"""


class Backoff(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        cpp = Path(cls.tmp.name) / "b.cpp"
        a = POLICY.index("static const uint32_t BACKOFF_FIRST_MS")
        b = POLICY.index("enum Outcome", a)
        body = "namespace rtel {\n" + POLICY[a:b] + "}\n"
        cpp.write_text("#include <cstddef>\n#include <cstdint>\n" + body + MAIN, encoding="utf-8")
        exe = compile_cpp(cpp, Path(cls.tmp.name) / ("b.exe" if os.name == "nt" else "b"))
        if exe is None:
            raise unittest.SkipTest("no host compiler")
        out = subprocess.run([str(exe)], capture_output=True, text=True, check=True).stdout
        cls.w = {"C": {}, "P": {}}
        for line in out.splitlines():
            kind, s, v = line.split()
            cls.w[kind].setdefault(int(s), []).append(int(v))

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def test_first_retry_within_a_minute(self):
        for kind in "CP":
            self.assertGreaterEqual(min(self.w[kind][0]), 30000)
            self.assertLessEqual(max(self.w[kind][0]), 60000)

    def test_doubles_then_caps_at_15_min_while_capturing(self):
        c = self.w["C"]
        for s in range(1, 4):
            self.assertLessEqual(max(c[s]), 60000 << s)
            self.assertGreaterEqual(min(c[s]), (60000 << s) // 2)
        for s in range(4, 12):
            self.assertLessEqual(max(c[s]), 15 * 60000, s)
            self.assertGreaterEqual(min(c[s]), 15 * 60000 // 2, s)

    def test_caps_at_60_min_while_paused(self):
        p = self.w["P"]
        self.assertLessEqual(max(max(v) for v in p.values()), 60 * 60000)
        for s in range(6, 12):
            self.assertGreaterEqual(min(p[s]), 30 * 60000, s)

    def test_waits_are_spread(self):
        for s in (0, 6):
            ws = self.w["C"][s]
            self.assertGreater(len(set(ws)), 1500, "too few distinct waits: gateways would retry together")
            self.assertGreater(max(ws) - min(ws), (max(ws) // 2) * 0.9)


if __name__ == "__main__":
    unittest.main(verbosity=2)
