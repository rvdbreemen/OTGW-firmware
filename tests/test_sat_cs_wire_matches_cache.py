#!/usr/bin/env python3
"""TASK-1738: the CS= / TC= text SAT puts on the wire equals the value it caches as "last sent".

satSendCS() and satEnqueueIfChangedCS() formatted the wire text with %.1f from the raw float but cached
lroundf(x * 10.0f) / 10. For a float just below x.x5 (45.5499992f, from flow 46.55 - offset 1.0, F1104
08:59:50) %.1f gives "45.5" while lroundf(455.5f) gives 456, so the 5 s repeat (satCmdResend ->
satSendCS(satCapRepeatCS(fLastSentCS))) sent CS=45.6, 0.1 C above what went out. For an exactly
representable x.x5 (20.25f, 45.25f) printf rounds half to even ("20.2") and lroundf half away (203): the
same split. Both the PIC and the OT-Direct path send through these helpers.

The code under test is NOT re-implemented: the test cuts the real CS senders out of
src/OTGW-firmware/SATcontrol.ino (from the TASK-1536 satCapRepeatCS comment to the Immergas TP block,
which holds satQuantizeTenths() once fixed, satCapRepeatCS, satResetCmdCache, satEnqueueOwnCS,
satEnqueueIfChangedCS, satSendCS and satPicCSKeepalive) and satEnqueueIfChangedTC(), and compiles them
with a host C++ compiler. Stubs only record the queued command text and provide the state structs.
Without a compiler the tests are skipped. OTGW_SATCONTROL_INO points at another copy (old-vs-fix runs).

Run: python tests/test_sat_cs_wire_matches_cache.py
"""
import os
import re
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import test_sat_pwm_override as base  # noqa: E402  (compile_cpp: g++, clang++ or MSVC)

REPO = Path(__file__).resolve().parent.parent
SRC = Path(os.environ.get("OTGW_SATCONTROL_INO", REPO / "src" / "OTGW-firmware" / "SATcontrol.ino"))

START = "// TASK-1536: a repeated CS (5 s resend, PIC keepalive)"
END = "// Immergas (SAT_QUIRK_IMMERGAS_TP)"

STUBS = r"""
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#define PSTR(x) x
#define snprintf_P snprintf
#define SATDebugTf(...) ((void)0)
static void satNarratef_P(const char*, ...) {}
enum class CmdQueueResult { Queued, Replaced, Dropped };
struct SatState {
  float fLastSentCS = 0; uint32_t iLastSentCSMs = 0, iLastSentMMMs = 0, iLastSentCHMs = 0; bool bLastSentValid = false;
  float fLastSentTC = 0; bool bTCAccountedValid = false; uint32_t iTCPushMs = 0; bool bPushRefused = false;
};
struct State { SatState sat; };
static State state;
static const uint32_t SAT_CMD_REFRESH_MS = 300000UL;
static bool _sat_mmSentValid = false, _sat_chSentValid = false, _sat_tcPushed = false;
static uint32_t g_now = 1000000;
static uint32_t millis() { return g_now; }
static float g_maxSp = 90.0f;
static float satEffectiveMaxSetpoint() { return g_maxSp; }
static bool isPICEnabled() { return true; }
static char g_last[32] = "";
static CmdQueueResult addCommandToQueue(const char* cmd, size_t, bool, int, bool) {
  strncpy(g_last, cmd, sizeof(g_last) - 1); return CmdQueueResult::Queued;
}
"""

MAIN = r"""
static float wire() { const char* e = strchr(g_last, '='); return e ? (float)atof(e + 1) : -1.0f; }
static int tenths(float v) { return (int)lroundf(v * 10.0f); }
int main(int argc, char** argv) {
  const char* c = argv[1]; float v = (float)atof(argv[2]);
  if (!strcmp(c, "send")) {                 // control tick: wire text vs cache
    satSendCS(v); printf("%s %d %d\n", g_last, tenths(wire()), tenths(state.sat.fLastSentCS));
  } else if (!strcmp(c, "repeat")) {        // tick, then the 5 s repeat path: must not change the wire value
    satSendCS(v); int first = tenths(wire());
    g_now += 5000; satSendCS(satCapRepeatCS(state.sat.fLastSentCS));
    printf("%s %d %d\n", g_last, first, tenths(wire()));
  } else if (!strcmp(c, "refresh")) {       // suppression refresh after a cached 0.1 lower value
    state.sat.fLastSentCS = (float)(tenths(v) - 1) / 10.0f; state.sat.bLastSentValid = true; state.sat.iLastSentCSMs = g_now;
    satEnqueueIfChangedCS(v); printf("%s %d %d\n", g_last, tenths(wire()), tenths(state.sat.fLastSentCS));
  } else if (!strcmp(c, "keepalive")) {     // capped PIC keepalive: wire text vs cache
    state.sat.fLastSentCS = 60.0f; state.sat.bLastSentValid = true; state.sat.iLastSentCSMs = g_now; g_maxSp = v;
    g_now += 31000; satPicCSKeepalive(); printf("%s %d %d\n", g_last, tenths(wire()), tenths(state.sat.fLastSentCS));
  } else if (!strcmp(c, "tc")) {
    satEnqueueIfChangedTC(v); printf("%s %d %d\n", g_last, tenths(wire()), tenths(state.sat.fLastSentTC));
  }
  return 0;
}
"""


def cut(src: str, a: str, b: str) -> str:
    i = src.find(a); j = src.find(b, i)
    if i < 0 or j < 0:
        raise AssertionError(f"marker not found: {a!r} / {b!r}; update the test")
    return src[i:j]


def cut_tc(src: str) -> str:
    m = re.search(r"static bool satEnqueueIfChangedTC\(float tc\) \{\n.*?\n\}\n", src, re.S)
    if not m:
        raise AssertionError("satEnqueueIfChangedTC() not found; update the test")
    return m.group(0)


START_FIX = "// TASK-1738: one rounding for a CS=/TC= value."   # the fix puts satQuantizeTenths() before START


def build(workdir: Path, src: str):
    cpp = workdir / "cs_wire.cpp"
    start = START_FIX if START_FIX in src else START
    cpp.write_text(STUBS + cut(src, start, END) + cut_tc(src) + MAIN, encoding="utf-8")
    return base.compile_cpp(cpp, workdir / ("cs_wire" + (".exe" if os.name == "nt" else "")))


def run(exe, *args):
    out = subprocess.run([str(exe)] + [str(a) for a in args], capture_output=True, text=True, check=True).stdout.split()
    return out[0], int(out[1]), int(out[2])


# 45.5499992f: F1104 08:59:50 (flow 46.55 - 1.0); 34.5499992f: 16:09:56 (35.55 - 1.0); 45.25f: exact x.x5
CS_VALUES = ("45.5499992", "34.5499992", "45.25", "45.5")


class CsWireMatchesCache(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        cls.exe = build(Path(cls.tmp.name), SRC.read_text(encoding="utf-8"))
        if cls.exe is None:
            raise unittest.SkipTest("no host C++ compiler (g++, clang++ or MSVC)")

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def test_tick_send_caches_what_went_on_the_wire(self):
        for v in CS_VALUES:
            text, wire, cache = run(self.exe, "send", v)
            self.assertEqual(wire, cache, (v, text))

    def test_five_second_repeat_resends_the_same_value(self):
        for v in CS_VALUES:
            text, first, again = run(self.exe, "repeat", v)
            self.assertEqual(first, again, (v, text))

    def test_suppression_refresh_caches_what_went_on_the_wire(self):
        for v in CS_VALUES:
            text, wire, cache = run(self.exe, "refresh", v)
            self.assertEqual(wire, cache, (v, text))

    def test_capped_keepalive_caches_what_went_on_the_wire(self):
        for v in CS_VALUES:
            text, wire, cache = run(self.exe, "keepalive", v)
            self.assertEqual(wire, cache, (v, text))

    def test_tc_push_caches_what_went_on_the_wire(self):
        for v in ("20.25", "19.5499992", "21.5"):
            text, wire, cache = run(self.exe, "tc", v)
            self.assertEqual(wire, cache, (v, text))


if __name__ == "__main__":
    unittest.main(verbosity=2)
