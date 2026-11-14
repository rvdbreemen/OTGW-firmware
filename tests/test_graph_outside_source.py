#!/usr/bin/env python3
"""TASK-1937: the 24 h graph ring (ADR-193) stores the weather value only while it is fresh.

    python tests/test_graph_outside_source.py                        # the working tree
    OTGW_GRAPH_REV=<sha> python tests/test_graph_outside_source.py   # another revision (old vs fix)

graphHistoryTick() picks the "outside" value of a slot: the weather API value when valid, else MsgID 27. Before the
fix "valid" was state.sat.weather.bValid alone, which only ever goes true (SATweather.ino), so after the weather fetch
stopped the ring kept writing the last value forever. SAT uses a freshness rule (SATcontrol.ino: bValid, a timestamp,
younger than SAT_OUTSIDE_VALID_MS = 1 h, Sergeant D entry 57); the ring now uses the same rule.

The code under test is sliced from the revision's src/OTGW-firmware/graphHistoryStuff.ino, from the
"// Outside follows the browser rule" comment up to the platformMutexLock(graphHistMutex) line, and compiled with that
revision's real graphHistoryStuff.h and jsonEmit.h and the Arduino/platform stubs of tests/stubs. Only the state it
reads is stubbed: the weather record, OTcurrentSystemState.Toutside, graphHistSeen(27) and millis().

Run: python tests/test_graph_outside_source.py
"""
import os
import re
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from test_sat_published_curve import compile_cpp  # noqa: E402

REPO = Path(__file__).resolve().parent.parent
FW = "src/OTGW-firmware/"
REV = os.environ.get("OTGW_GRAPH_REV")


def read(path):
    if REV is None:
        return (REPO / path).read_text(encoding="utf-8").replace("\r\n", "\n")
    return subprocess.run(["git", "-C", str(REPO), "show", f"{REV}:{path}"], capture_output=True, text=True,
                          encoding="utf-8", check=True).stdout.replace("\r\n", "\n")


HARNESS = r"""
#include "Arduino.h"
#include "jsonEmit.h"
#include "graphHistoryStuff.h"
#include <cstdio>

struct { struct { struct { bool bValid; float fTemperature; uint32_t iLastUpdateMs; } weather; } sat; } state;
struct { float Toutside; } OTcurrentSystemState;
static bool g_otSeen = false;
static uint32_t g_now = 0;
static bool graphHistSeen(uint8_t id) { return id == 27 && g_otSeen; }
static uint32_t millis_() { return g_now; }
#define millis millis_

static int16_t outsideOf(bool valid, float wx, uint32_t ageMs, bool everFetched, float ot, bool otSeen) {
  g_now = 10UL * 3600000UL;   // ten hours after boot
  state.sat.weather.bValid = valid;
  state.sat.weather.fTemperature = wx;
  state.sat.weather.iLastUpdateMs = everFetched ? g_now - ageMs : 0;
  OTcurrentSystemState.Toutside = ot;
  g_otSeen = otSeen;
  GraphHistSlot s = graphHistEmptySlot();
@@SLICE@@
  return s.outside;
}

int main() {
  const int16_t WX = graphHistEncTemp(4.5f, true), OT = graphHistEncTemp(9.0f, true), NONE = graphHistEncTemp(0, false);
  printf("fresh|%d\n", outsideOf(true, 4.5f, 10UL * 60000UL, true, 9.0f, true) == WX);
  printf("stale_ot|%d\n", outsideOf(true, 4.5f, 2UL * 3600000UL, true, 9.0f, true) == OT);
  printf("stale_none|%d\n", outsideOf(true, 4.5f, 2UL * 3600000UL, true, 9.0f, false) == NONE);
  printf("never|%d\n", outsideOf(false, 0.0f, 0, false, 9.0f, true) == OT);
  printf("valid_no_stamp|%d\n", outsideOf(true, 4.5f, 0, false, 9.0f, true) == OT);
  printf("edge_59min|%d\n", outsideOf(true, 4.5f, 3599000UL, true, 9.0f, true) == WX);
  return 0;
}
"""


class GraphOutsideSource(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        ino = read(FW + "graphHistoryStuff.ino")
        a = ino.find("  // Outside follows the browser rule")
        b = ino.find("  platformMutexLock(graphHistMutex);", a)
        if a < 0 or b < 0:
            raise AssertionError("outside-source block not found in graphHistoryTick()")
        cls.tmp = tempfile.TemporaryDirectory()
        d = Path(cls.tmp.name)
        for stub in ("Arduino.h", "platform.h"):
            shutil.copy(REPO / "tests" / "stubs" / stub, d / stub)
        for h in ("graphHistoryStuff.h", "jsonEmit.h"):
            # the shared compile helper takes no include path: quoted includes find the copies in this directory
            text = re.sub(r"#include\s*<(Arduino|platform)\.h>", r'#include "\1.h"', read(FW + h))
            (d / h).write_text(text, encoding="utf-8")
        cpp = d / "t.cpp"
        cpp.write_text(HARNESS.replace("@@SLICE@@", ino[a:b]), encoding="utf-8")
        exe = compile_cpp(cpp, d / "t.exe")
        if exe is None:
            raise unittest.SkipTest("no host C++ compiler")
        out = subprocess.run([str(exe)], capture_output=True, text=True, check=True).stdout
        print(out)
        cls.rows = dict(l.split("|", 1) for l in out.splitlines())

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def test_fresh_weather_value_is_stored(self):
        self.assertEqual(self.rows["fresh"], "1")
        self.assertEqual(self.rows["edge_59min"], "1")

    def test_stale_weather_value_falls_back_to_msgid_27(self):
        self.assertEqual(self.rows["stale_ot"], "1", "a weather value two hours old was stored")

    def test_stale_weather_without_msgid_27_stores_no_value(self):
        self.assertEqual(self.rows["stale_none"], "1")

    def test_never_fetched_or_no_timestamp_uses_msgid_27(self):
        self.assertEqual(self.rows["never"], "1")
        self.assertEqual(self.rows["valid_no_stamp"], "1")

    @unittest.skipIf(REV is not None, "constant check runs on the working tree")
    def test_window_equals_sat_rule(self):
        h = read(FW + "graphHistoryStuff.h")
        sat = read(FW + "SATcontrol.ino")
        g = re.search(r"#define GRAPH_HIST_WEATHER_VALID_MS\s+(\d+)UL", h)
        s = re.search(r"SAT_OUTSIDE_VALID_MS\s*=\s*(\d+)UL", sat)
        self.assertTrue(g and s, "GRAPH_HIST_WEATHER_VALID_MS or SAT_OUTSIDE_VALID_MS not found")
        self.assertEqual(g.group(1), s.group(1), "the ring and SAT use different freshness windows")


if __name__ == "__main__":
    unittest.main(verbosity=2)
