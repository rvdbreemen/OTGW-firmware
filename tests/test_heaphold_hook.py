#!/usr/bin/env python3
"""TASK-2026 bench hook: telnet 'H' starves the heap on purpose (prerelease builds only), and releases it.

The REAL block between '// --- heapHold begin' and '// --- heapHold end' of handleDebug.ino is compiled on the host
against a fake heap: one large region plus small fragments; malloc() takes from the region while it fits, then from
the fragments; the largest block is the larger of the region and the biggest fragment (2 KB). Checked: the key only
raises a request and the loop tick takes the heap down to the starve state of .61 (free under 7 KB, largest block
under 4 KB) without going under the 4 KB floor; the key again releases every block; the timer releases after 30 min;
a heap already near the floor is not taken further; while held, the 5 s re-hold check takes more when the heap
recovers (free stays under 8192, largest block under 4096) and gives blocks back when another consumer pushes free
under the floor. Those two re-hold cases are red on 24679e271 (free 17040 after a 12 KB recovery; free 3000 after a
squeeze) and green on the re-hold hook.
Run: python tests/test_heaphold_hook.py
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

SRC = (HERE.parent / "src" / "OTGW-firmware" / "handleDebug.ino").read_text(encoding="utf-8").replace("\r\n", "\n")

PRE = r"""
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <atomic>
#include <vector>
typedef const char* PGM_P;
#define PSTR(s) (s)
#define DebugTf(...) printf(__VA_ARGS__)
static uint32_t g_ms = 1000;
static uint32_t millis() { return g_ms; }
// fake heap: one region (taken first) plus fragments of at most 2 KB
static uint32_t g_region = 31000, g_frag = 15000;
static const uint32_t FRAG_MAX = 2048;
static uint32_t g_start = 0;
static std::vector<std::pair<void*, uint32_t>> g_live;   // what the hook got, to give it back on free()
static uint32_t platformFreeHeap() { return g_region + g_frag; }
static uint32_t g_lowWater = 0xFFFFFFFFu;   // since-boot low-water mark, as heap_caps_get_minimum_free_size()
static void noteLow() { if (g_region + g_frag < g_lowWater) g_lowWater = g_region + g_frag; }
static uint32_t platformMinFreeHeap() { noteLow(); return g_lowWater; }
static uint32_t platformMaxFreeBlock() { uint32_t f = g_frag < FRAG_MAX ? g_frag : FRAG_MAX; return g_region > f ? g_region : f; }
static void* fakeMalloc(size_t n) {
  if (n <= g_region) g_region -= (uint32_t)n;
  else if (n <= FRAG_MAX && n <= g_frag) g_frag -= (uint32_t)n;
  else return nullptr;
  noteLow(); void* p = std::malloc(1); g_live.push_back({p, (uint32_t)n}); return p;
}
static void fakeFree(void* p) {
  for (size_t i = 0; i < g_live.size(); i++) if (g_live[i].first == p) {
    g_region += g_live[i].second; std::free(p); g_live.erase(g_live.begin() + i); return; }
}
#define malloc(n) fakeMalloc(n)
#define free(p) fakeFree(p)
#define _VERSION_PRERELEASE beta.0
"""

MAIN = r"""
int main(int argc, char** argv) {
  const char* c = argc > 1 ? argv[1] : "";
  if (!strcmp(c, "near_floor")) { g_region = 1000; g_frag = 3500; }
  g_start = platformFreeHeap();
  printf("start free=%u maxblk=%u\n", platformFreeHeap(), platformMaxFreeBlock());
  s_heapHoldReq = 1;                       // the key (async_tcp)
  printf("before_tick n=%u\n", (unsigned)s_heapHoldN);
  heapHoldTick();                          // the loop task
  printf("held n=%u free=%u maxblk=%u\n", (unsigned)s_heapHoldN, platformFreeHeap(), platformMaxFreeBlock());
  if (!strcmp(c, "key")) { s_heapHoldReq = 1; heapHoldTick(); }
  if (!strcmp(c, "recover")) {            // another consumer gives back 12 KB while held (.61: free rose above 8192)
    g_region += 12000; printf("recovered free=%u maxblk=%u\n", platformFreeHeap(), platformMaxFreeBlock());
    g_ms += 1000; heapHoldTick(); printf("at1s free=%u maxblk=%u\n", platformFreeHeap(), platformMaxFreeBlock());
    g_ms += 5000; heapHoldTick(); printf("at6s free=%u maxblk=%u\n", platformFreeHeap(), platformMaxFreeBlock()); }
  if (!strcmp(c, "squeeze")) {            // another consumer takes heap while held: the floor must hold again
    uint32_t fr = platformFreeHeap(), take = fr - 3000;
    uint32_t r = take < g_region ? take : g_region; g_region -= r; g_frag -= (take - r); noteLow();
    g_ms += 1000; heapHoldTick(); printf("squeezed free=%u n=%u\n", platformFreeHeap(), (unsigned)s_heapHoldN); }
  if (!strcmp(c, "timer")) { g_ms += 29UL * 60000UL; heapHoldTick(); printf("at29 n=%u\n", (unsigned)s_heapHoldN);
                             g_ms += 60000UL; heapHoldTick();
                             for (int m = 0; m < 5; m++) { g_ms += 60000UL; heapHoldTick(); } }   // summary repeats
  printf("end n=%u free=%u back=%d\n", (unsigned)s_heapHoldN, platformFreeHeap(), (int)(platformFreeHeap() == g_start));
  return 0;
}
"""


def block():
    a = SRC.index("// --- heapHold begin")
    b = SRC.index("// --- heapHold end", a)
    return SRC[a:b]


class HeapHoldHook(unittest.TestCase):
    exe = None

    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        cpp = Path(cls.tmp.name) / "h.cpp"
        cpp.write_text(PRE + block() + MAIN, encoding="utf-8")
        cls.exe = compile_cpp(cpp, Path(cls.tmp.name) / ("h.exe" if os.name == "nt" else "h"))
        if cls.exe is None:
            raise unittest.SkipTest("no host compiler")

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def run_c(self, c):
        out = subprocess.run([str(self.exe), c], capture_output=True, check=True).stdout.decode().replace("\r", "")
        keys = ("start", "before_tick", "held", "recovered", "at1s", "at6s", "squeezed", "at29", "end")   # the hook's own log lines are not key=value rows
        rows = [l for l in out.splitlines() if l.split(" ", 1)[0] in keys]
        return dict((l.split(" ", 1)[0], dict(kv.split("=") for kv in l.split()[1:])) for l in rows), out

    def test_the_key_only_queues_and_the_loop_starves_to_the_target(self):
        r, out = self.run_c("key")
        self.assertEqual(r["before_tick"]["n"], "0", "the key itself allocated (async_tcp)")
        held = r["held"]
        self.assertGreater(int(held["n"]), 0, out)
        self.assertLess(int(held["free"]), 7168, out)
        self.assertGreaterEqual(int(held["free"]), 4096, "went under the floor: " + out)
        self.assertLess(int(held["maxblk"]), 4096, out)
        # TASK-2026 (388): the target is 2048 B, clearly under WIFI_STARVED_MAXBLOCK (the 386 hold ended at 4084 B on
        # .61, on the edge). This fake heap keeps its fragments at up to 2 KB in one pool, so it cannot get below 2048
        # without crossing the floor: the floor wins (test_a_heap_near_the_floor_is_not_taken_further). Whether the real
        # heap gets under 2048 is read on the bench from the hook's own "heaphold: ... maxblk=" line.

    def test_the_key_again_releases_every_block(self):
        r, out = self.run_c("key")
        self.assertEqual(r["end"]["n"], "0", out)
        self.assertEqual(r["end"]["back"], "1", "not all heap given back: " + out)
        self.assertIn("heaphold released (key)", out)

    def test_the_timer_releases_after_30_min(self):
        r, out = self.run_c("timer")
        self.assertNotEqual(r["at29"]["n"], "0", "released before 30 min: " + out)
        self.assertEqual(r["end"]["n"], "0", out)
        self.assertIn("heaphold released (timer)", out)

    def test_the_hold_stays_starved_when_the_heap_recovers(self):
        """TASK-2026 (coordinator, Robert "B"): during the WHOLE hold free < 8192 (HEAP_LOW_THRESHOLD) and the largest
        block < 4096 (WIFI_STARVED_MAXBLOCK). Red on 24679e271: the hook took once, so free rose with the heap."""
        r, out = self.run_c("recover")
        self.assertGreaterEqual(int(r["recovered"]["free"]), 8192, "the test did not raise the heap: " + out)
        self.assertGreater(int(r["at1s"]["free"]), 8000, "topped up before the re-hold interval: " + out)
        self.assertLess(int(r["at6s"]["free"]), 8192, out)
        self.assertLess(int(r["at6s"]["maxblk"]), 4096, out)
        self.assertGreaterEqual(int(r["at6s"]["free"]), 4096, "went under the floor: " + out)

    def test_the_hold_summary_survives_a_dead_telnet(self):
        """391 bench (.61): telnet was dead for the whole hold and the banner's since-boot minFree (1040) could not be
        told apart from the hook's own floor. The release prints a summary with the lowest free the hook sampled and the
        low-water mark at take and release, and repeats it once a minute 3 times. Red on ebc0cf98b (no summary)."""
        r, out = self.run_c("timer")
        lines = [l for l in out.splitlines() if l.startswith("heaphold summary:")]
        self.assertEqual(len(lines), 4, out)          # at the release + 3 repeats, not more
        self.assertIn("lowest free seen by the hook", lines[0])
        self.assertIn("minFree since boot", lines[0])
        self.assertIn("(was ", lines[0])

    def test_the_floor_holds_when_another_consumer_takes_heap(self):
        """The floor is checked on every loop pass, not only every 5 s: 1 s after the squeeze it holds again. Red on
        ebc0cf98b (give-back only in the 5 s re-hold check)."""
        r, out = self.run_c("squeeze")
        self.assertGreaterEqual(int(r["squeezed"]["free"]), 4096, out)
        self.assertGreater(int(r["squeezed"]["n"]), 0, "gave back everything instead of only what the floor needs: " + out)

    def test_a_heap_near_the_floor_is_not_taken_further(self):
        r, out = self.run_c("near_floor")
        self.assertGreaterEqual(int(r["held"]["free"]), 4096, out)

    def test_prerelease_only(self):
        a = SRC.index("// --- heapHold begin")
        self.assertIn("#if defined(_VERSION_PRERELEASE)", SRC[max(0, a - 80):a])
        k = SRC.index("case 'H':")
        self.assertIn("#if defined(_VERSION_PRERELEASE)", SRC[max(0, k - 60):k])


if __name__ == "__main__":
    unittest.main(verbosity=2)
