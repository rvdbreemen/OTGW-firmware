#!/usr/bin/env python3
"""TASK-1386: heap tier-entry counters also count a dip that only the allocator low-water mark saw.

Nothing is re-implemented: the test cuts the real code out of src/OTGW-firmware/helperStuff.ino, from the
HEAP_CRITICAL_THRESHOLD define up to the end of getHeapHealth(), and compiles it with a host C++ compiler against
stubs for platformFreeHeap() / platformMinFreeHeap() and state.heapdiag. OTGW_HELPER_INO points at another copy
(e.g. `git show HEAD~1:src/OTGW-firmware/helperStuff.ino`) for an old-vs-fix run: before TASK-1386 a dip between two
calls counts nothing, so the watermark cases fail there.

Bench case (alpha.570, OTGW32): min free heap 2240 B with entered_critical 0.

Run: python tests/test_heap_tier_watermark.py
"""
import os
import re
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(Path(__file__).resolve().parent))
from test_sat_pwm_override import compile_cpp  # noqa: E402

SRC = Path(os.environ.get("OTGW_HELPER_INO", REPO / "src" / "OTGW-firmware" / "helperStuff.ino"))

STUBS = r"""
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#define PSTR(x) x
#define DebugTf(...) ((void)0)
enum HeapHealthLevel { HEAP_HEALTHY = 0, HEAP_LOW = 1, HEAP_WARNING = 2, HEAP_CRITICAL = 3 };
struct HeapDiag { uint32_t iWsDropsTotal = 0, iMqttDropsTotal = 0, iEnteredLowCount = 0, iEnteredWarningCount = 0,
                  iEnteredCriticalCount = 0, iEnteredFromWatermark = 0; };
struct State { HeapDiag heapdiag; };
static State state;
static uint32_t g_free = 100000, g_min = 100000;
static uint32_t platformFreeHeap() { return g_free; }
static uint32_t platformMinFreeHeap() { return g_min; }
static uint32_t platformMaxFreeBlock() { return 31732; }
static uint32_t millis() { return 0; }
"""

MAIN = r"""
// argv: pairs of free,min per call
int main(int argc, char** argv) {
  for (int i = 1; i + 1 < argc; i += 2) {
    g_free = (uint32_t)atol(argv[i]); g_min = (uint32_t)atol(argv[i + 1]);
    getHeapHealth();
  }
  printf("low=%u warn=%u crit=%u wm=%u\n", state.heapdiag.iEnteredLowCount, state.heapdiag.iEnteredWarningCount,
         state.heapdiag.iEnteredCriticalCount, state.heapdiag.iEnteredFromWatermark);
  return 0;
}
"""


def extract(src):
    a = src.index("#define HEAP_CRITICAL_THRESHOLD")
    i = src.index("HeapHealthLevel getHeapHealth()", a)
    j = src.index("\n}\n", i) + 3
    body = src[a:j]
    # drop the drop-accounting statics that need more stubs than the function uses
    return "\n".join(l for l in body.splitlines() if not re.match(r"\s*static uint32_t (lastWebSocketWarningMs|lastMQTTWarningMs|webSocketDropCount|mqttDropCount)", l))


class HeapTierWatermark(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        cpp = Path(cls.tmp.name) / "t.cpp"
        cpp.write_text(STUBS + extract(SRC.read_text(encoding="utf-8").replace("\r\n", "\n")) + MAIN, encoding="utf-8")
        cls.exe = compile_cpp(cpp, Path(cls.tmp.name) / "t.exe")

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def run_calls(self, *pairs):
        if not self.exe:
            self.skipTest("no host C++ compiler")
        args = [str(v) for p in pairs for v in p]
        out = subprocess.run([str(self.exe), *args], capture_output=True, text=True, check=True).stdout
        return dict(kv.split("=") for kv in out.split())

    def test_bench_case_dip_to_2240_between_calls_counts_every_tier_once(self):
        # sampling only ever sees a healthy heap; the watermark says it dipped to 2240 B
        r = self.run_calls((70000, 70000), (71000, 2240), (71000, 2240))
        self.assertEqual((r["low"], r["warn"], r["crit"], r["wm"]), ("1", "1", "1", "3"))

    def test_dip_to_4000_counts_low_and_warning_not_critical(self):
        r = self.run_calls((70000, 4000))
        self.assertEqual((r["low"], r["warn"], r["crit"], r["wm"]), ("1", "1", "0", "2"))

    def test_sampled_entry_is_not_counted_again_from_the_watermark(self):
        # sampling sees CRITICAL itself (free 2000, watermark 2000), then recovers: one critical entry, none from wm
        r = self.run_calls((2000, 2000), (70000, 2000), (70000, 2000))
        self.assertEqual((r["crit"], r["wm"]), ("1", "2"))   # LOW and WARNING were skipped by the jump: wm adds them

    def test_repeated_sampled_entries_still_count(self):
        r = self.run_calls((2000, 2000), (70000, 2000), (2500, 2000), (70000, 2000))
        self.assertEqual(r["crit"], "2")

    def test_healthy_heap_counts_nothing(self):
        r = self.run_calls((70000, 60000), (65000, 50000))
        self.assertEqual((r["low"], r["warn"], r["crit"], r["wm"]), ("0", "0", "0", "0"))


if __name__ == "__main__":
    unittest.main()
