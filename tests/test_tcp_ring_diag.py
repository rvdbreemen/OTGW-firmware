"""TASK-3119 follow-up: the prerelease RAM ring with one lwIP / heap sample per second (tcpRingDiag.h).

Reads the REAL header, compiled on the host, twice: with _VERSION_PRERELEASE (the ring and its rules) and without (nothing survives).
Checks: sample size 12 bytes, push order and wrap-around at 240, count caps at 240, quantisation to 16 bytes and clamping (heap, block,
pcb counts), the armed rule (a sample under 8192 B free), the empty-ring sentinel. Text checks on the wiring: the 1 Hz sampler pushes the
ring, the telnet key 'T' only asks for a dump, both inside _VERSION_PRERELEASE regions, and the dump is printed by the loop task side
(tcpRingTick), not from the telnet handler. What it cannot reach: lwIP itself and the stall (bench: K hold, then telnet key T).
"""
import os
import re
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
SRC = REPO / "src" / "OTGW-firmware"
sys.path.insert(0, str(Path(__file__).resolve().parent))
from test_sat_pwm_override import compile_cpp  # noqa: E402

MAIN_PRE = r"""
#include <cstdio>
#include "tcpRingDiag.h"
void tcpRingRequestDump() {}
int main() {
  static TcpRing r;
  std::printf("size %zu n %u empty_min %u armed %d\n", sizeof(TcpRingSample), (unsigned)TCP_RING_N, (unsigned)tcpRingMinFree(r), (int)tcpRingArmed(r));
  // 300 pushes: the ring keeps the newest 240, oldest first
  for (uint32_t i = 1; i <= 300; i++) tcpRingPush(r, i, 40000, 20000, 6, 2, 1);
  std::printf("count %u first %u last %u\n", (unsigned)r.count, (unsigned)tcpRingAt(r, 0).upSec, (unsigned)tcpRingAt(r, r.count - 1).upSec);
  std::printf("armed_high %d\n", (int)tcpRingArmed(r));
  // a dip under 8192: armed; the minimum is reported in 16-byte units
  tcpRingPush(r, 301, 1600, 1024, 9, 3, 2);
  std::printf("armed_dip %d min %u\n", (int)tcpRingArmed(r), (unsigned)tcpRingMinFree(r));
  // exactly 8192 is not under the arm line
  static TcpRing q;
  tcpRingPush(q, 1, 8192, 4096, 1, 1, 1);
  std::printf("at_line %d\n", (int)tcpRingArmed(q));
  tcpRingPush(q, 2, 8191, 4096, 1, 1, 1);
  std::printf("under_line %d\n", (int)tcpRingArmed(q));
  // clamping: huge heap, huge counts
  static TcpRing c;
  tcpRingPush(c, 7, 4000000000u, 4000000000u, 1000, 1000, 1000);
  const TcpRingSample& s = tcpRingAt(c, 0);
  std::printf("clamp q %u %u pcb %u %u %u\n", (unsigned)s.freeQ, (unsigned)s.maxBlockQ, (unsigned)s.active, (unsigned)s.timeWait, (unsigned)s.tx80);
  return 0;
}
"""

MAIN_STABLE = r"""
#include <cstdio>
#include "tcpRingDiag.h"
int main() {
#if defined(TCP_RING_N)
  std::printf("LEAK\n"); return 1;
#endif
  std::printf("EMPTY\n");
  return 0;
}
"""


def lines(out):
    d = {}
    for ln in out.splitlines():
        k, *rest = ln.split()
        d[k] = rest
    return d


class Header(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        (Path(cls.tmp.name) / "tcpRingDiag.h").write_text((SRC / "tcpRingDiag.h").read_text(encoding="utf-8"), encoding="utf-8")

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def run_cpp(self, name, main, define):
        cpp = Path(self.tmp.name) / (name + ".cpp")
        cpp.write_text(define + main, encoding="utf-8")
        exe = compile_cpp(cpp, Path(self.tmp.name) / (name + (".exe" if os.name == "nt" else "")))
        if exe is None:
            self.skipTest("no host C++ compiler")
        r = subprocess.run([str(exe)], capture_output=True, text=True)
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        return r.stdout

    def test_ring_behaviour(self):
        o = lines(self.run_cpp("pre", MAIN_PRE, '#define _VERSION_PRERELEASE "beta.1"\n'))
        self.assertEqual(o["size"], ["12", "n", "240", "empty_min", "4294967295", "armed", "0"])
        self.assertEqual(o["count"], ["240", "first", "61", "last", "300"])
        self.assertEqual(o["armed_high"], ["0"])
        self.assertEqual(o["armed_dip"], ["1", "min", "1600"])
        self.assertEqual(o["at_line"], ["0"])
        self.assertEqual(o["under_line"], ["1"])
        self.assertEqual(o["clamp"], ["q", "65535", "65535", "pcb", "255", "255", "255"])

    def test_stable_build_has_nothing(self):
        self.assertIn("EMPTY", self.run_cpp("stable", MAIN_STABLE, ""))


class Wiring(unittest.TestCase):
    def test_sampler_pushes_the_ring_once_per_second_inside_the_prerelease_guard(self):
        s = (SRC / "helperStuff.ino").read_text(encoding="utf-8").replace("\r\n", "\n")
        m = re.search(r"^void sampleHeapWatermark\(\) \{.*?^\}\s*$", s, re.S | re.M)
        self.assertIsNotNone(m)
        body = m.group(0)
        self.assertIn("tcpRingTick();", body)
        region = re.search(r"#if defined\(_VERSION_PRERELEASE\)\n\s*tcpRingTick\(\);[^\n]*\n#endif", body)
        self.assertIsNotNone(region, "tcpRingTick() must sit inside its own _VERSION_PRERELEASE region")
        tick = re.search(r"^void tcpRingTick\(\) \{.*?^\}\s*$", s, re.S | re.M)
        self.assertIsNotNone(tick)
        for needle in ("tcpRingPush(", "platformTcpHoldSnapshot(", "platformFreeHeap()", "platformMaxFreeBlock()", "state.uptime.iSeconds"):
            self.assertIn(needle, tick.group(0), needle)

    def test_telnet_key_t_only_asks_for_a_dump_inside_the_prerelease_guard(self):
        s = (SRC / "handleDebug.ino").read_text(encoding="utf-8").replace("\r\n", "\n")
        m = re.search(r"#if defined\(_VERSION_PRERELEASE\)\n\s*case 'H':.*?#endif", s, re.S)
        self.assertIsNotNone(m)
        self.assertRegex(m.group(0), r"(?s)case 'T':.*?tcpRingRequestDump\(\);")
        # the telnet handler must not walk or print the ring itself
        self.assertNotIn("tcpRingAt(", s)

    def test_header_is_included_once(self):
        h = (SRC / "OTGW-firmware.h").read_text(encoding="utf-8")
        self.assertEqual(h.count('#include "tcpRingDiag.h"'), 1)


if __name__ == "__main__":
    unittest.main()
