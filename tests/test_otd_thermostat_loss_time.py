#!/usr/bin/env python3
"""TASK-1802 bug 9: OT-Direct counts a lost thermostat as lost after about 3 s, not after iSetbackTimeout (30 s).

    python tests/test_otd_thermostat_loss_time.py                       # the working tree
    OTGW_OTD_REV=<sha> python tests/test_otd_thermostat_loss_time.py     # another revision (old vs fix)

In gateway mode the gateway sends its own frames to the boiler only while bThermostatConnected is false
(otdOwnFramesAllowed). checkThermostatTimeout() clears it; before this fix it waited iSetbackTimeout seconds (default 30,
OTDirecttypes.h), so the boiler got no frame for 30 s after the thermostat disappeared, against the spec's 1.15 s master
interval (spec v4.2 :1101) and its 5 s "no valid frame" limit (:1006-1016). The PIC sees the open line within tens of
ms (gateway.asm 6.6 CheckThermostat :1728-1816). Robert decided A on 2026-11-15 (Discord DM 1571431149795024929):
about 3 s, fixed.

REAL code: checkThermostatTimeout() is sliced from the revision's OTDirect.ino with the harness of
tests/test_otd_thermostat_lines.py (stubs, fake 1 s loop). Each case is its own process. The stub keeps
iSetbackTimeout = 30, the shipped default, so the old code shows the 30 s.
"""
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import test_otd_thermostat_lines as tl  # noqa: E402
from test_sat_published_curve import compile_cpp  # noqa: E402

MAIN = r"""
int main(int argc, char **argv) {
  const std::string c = argc > 1 ? argv[1] : "";
  if (c == "loss") {            // thermostat sends every second for 20 s, then stops: seconds until it counts as lost
    for (int i = 0; i < 20; i++) tick(true);
    int s = 0;
    while (state.otd.bThermostatConnected && s < 120) { tick(false); s++; }
    printf("loss|%d|%d\n", s, (int)state.otd.bThermostatConnected);
  } else if (c == "gaps") {     // a thermostat with up to 2 s between frames never counts as lost
    int lost = 0;
    for (int i = 0; i < 60; i++) { tick(i % 2 == 0); if (!state.otd.bThermostatConnected) lost++; }
    printf("gaps|%d|%d\n", lost, (int)state.otd.bThermostatConnected);
  } else if (c == "ovr" || c == "noovr") {   // TASK-1804 A3: a TT=/TC= override is forgotten with the thermostat
    if (c == "ovr") otRemoteOverride.mode = OT_OVERRIDE_TEMPORARY;
    for (int i = 0; i < 20; i++) tick(true);
    for (int i = 0; i < 10; i++) tick(false);
    printf("%s|%d|%d\n", c.c_str(), g_overrideClears, (int)otRemoteOverride.mode);
  } else if (c == "return") {   // lost, then one frame: connected again at once
    for (int i = 0; i < 20; i++) tick(true);
    for (int i = 0; i < 10; i++) tick(false);
    tick(true);
    printf("return|0|%d\n", (int)state.otd.bThermostatConnected);
  }
  return 0;
}
"""


class ThermostatLossTime(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        src = tl.read()
        a = src.find("// checkThermostatTimeout")
        a = src.rfind("// -----", 0, a)
        b = src.find("\n}\n", src.find("static void checkThermostatTimeout()", a))
        if a < 0 or b < 0:
            raise AssertionError("checkThermostatTimeout() not found in OTDirect.ino")
        harness = tl.HARNESS.replace("@@SLICE@@", src[a:b + 3])
        harness = harness[:harness.index("int main(")] + MAIN
        cls.tmp = tempfile.TemporaryDirectory()
        cpp = Path(cls.tmp.name) / "t.cpp"
        cpp.write_text(harness, encoding="utf-8")
        exe = compile_cpp(cpp, Path(cls.tmp.name) / "t.exe")
        if exe is None:
            raise unittest.SkipTest("no host C++ compiler")
        cls.rows = {}
        for case in ("loss", "gaps", "return", "ovr", "noovr"):
            out = subprocess.run([str(exe), case], capture_output=True, text=True, check=True).stdout.strip()
            print(out)
            name, n, connected = out.split("|")
            cls.rows[name] = (int(n), connected == "1")

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def test_a_lost_thermostat_counts_as_lost_within_about_3_s(self):
        seconds, connected = self.rows["loss"]
        self.assertFalse(connected, "the thermostat must count as lost")
        self.assertGreaterEqual(seconds, 3, "not before 3 s: a thermostat sends at least every 1.15 s")
        self.assertLessEqual(seconds, 4, "Robert's A: about 3 s (checked once a second), not iSetbackTimeout 30 s")

    def test_a_thermostat_with_2_s_gaps_never_counts_as_lost(self):
        lost, connected = self.rows["gaps"]
        self.assertEqual(lost, 0)
        self.assertTrue(connected)

    def test_a_tt_override_is_forgotten_with_the_thermostat(self):
        # TASK-1804 A3: PIC Disconnect clears override/setpoint1/setpoint2 (gateway.asm 6.8 :1786-1788)
        self.assertEqual(self.rows["ovr"], (1, False), "one clearRemoteOverride(), override gone")
        self.assertEqual(self.rows["noovr"][0], 0, "no clear without an override")

    def test_one_frame_after_a_loss_reconnects(self):
        self.assertTrue(self.rows["return"][1])


GAP_MAIN = r"""
// gateway mode: the thermostat sends every second until 20 s, then stops. checkThermostatTimeout() runs once a second
// (as in loopOTDirect), the scheduler every 10 ms, the boiler answers in 50 ms. Measures the gap from the thermostat's
// last frame to the gateway's first own frame.
int main() {
  otCurrentMode = OTD_MODE_GATEWAY;
  for (int i = 0; i < OT_SCHEDULE_SIZE; i++) otSchedule[i] = { 0, 60000, 0, true, false, false, 0 };
  otSchedule[0] = { 0, OT_STATUS_INTERVAL_MS, 0, false, false, false, 0 };
  state.otBus.bOnline = true;
  state.otd.bThermostatConnected = false;
  const uint32_t lastFrame = 20000;
  uint32_t reqAt = 0;
  int ownBefore = 0;
  for (g_now = 1000; g_now < 70000; g_now += 10) {
    if (g_now <= lastFrame && g_now % 1000 == 0) {          // as handleSlaveRequest() (OTDirect.ino, thermostat frame)
      otThermostatSeen = true; otLastThermostatMs = g_now; state.otd.bThermostatConnected = true;
    }
    if (g_now % 1000 == 500) checkThermostatTimeout();
    if (otMasterRequestActive) {
      if (g_now - reqAt >= 50) otMasterRequestActive = false;
      continue;
    }
    const int before = g_sent;
    scheduleMasterRequest();
    if (g_sent != before) {
      reqAt = g_now; otLastAnySendMs = g_now;
      if (g_now <= lastFrame) ownBefore++;
    }
  }
  int first = -1;
  for (int i = 0; i < g_sent && i < 4096; i++) if (g_sentAt[i] > lastFrame) { first = i; break; }
  printf("gap|%d|%d\n", first < 0 ? -1 : (int)(g_sentAt[first] - lastFrame), ownBefore);
  return 0;
}
"""


def build_gap_source(src):
    import test_otd_offline_retry as ofr
    full = ofr.build_source()
    full = full[:full.index(ofr.MAIN)]
    old = "struct { struct { bool bSummerMode = false; bool bEnableSlave = false; } otd; } settings;"
    assert old in full, "scheduler prelude changed: settings stub"
    full = full.replace(old, "struct { struct { bool bSummerMode = false; bool bEnableSlave = false; "
                             "uint8_t iSetbackTimeout = 30; } otd; } settings;")
    # the prelude stubs the own-frames gate as always true; use the real one (markers in OTDirect.ino)
    stub = "static bool otdOwnFramesAllowed(bool, bool) { return true; }"
    assert full.count(stub) == 1, "scheduler prelude changed: otdOwnFramesAllowed stub"
    g0 = src.index("// --- otdOwnStatusAllowed begin")
    gate = src[g0:src.index("// --- otdOwnStatusAllowed end", g0)]
    full = full.replace(stub, gate)
    stubs = r"""
#include <cstring>
typedef const char *PGM_P;
static size_t strlcpy_P(char *d, const char *s, size_t n) { size_t l = strlen(s); if (n) { size_t c = l < n - 1 ? l : n - 1; memcpy(d, s, c); d[c] = 0; } return l; }
static void otDirectBridgeWriteLine(const char *, size_t) {}
static void processOT(const char *, int) {}
static void otdForgetThermostatWrites() {}
static void clearRemoteOverride() { otRemoteOverride.mode = OT_OVERRIDE_NONE; }
"""
    a = src.find("// checkThermostatTimeout")
    a = src.rfind("// -----", 0, a)
    b = src.find("\n}\n", src.find("static void checkThermostatTimeout()", a))
    return full + stubs + src[a:b + 3] + "\n" + GAP_MAIN


class GapToOwnFrame(unittest.TestCase):
    """AC2 of TASK-1802: the gap from the thermostat's last frame to the gateway's first own frame, on the real
    checkThermostatTimeout() + scheduleMasterRequest() + otdOwnFramesAllowed() (scheduler harness of
    tests/test_otd_offline_retry.py). Spec v4.2 :1006-1016: no valid frame for 5 s or more may be read as a short."""

    @classmethod
    def setUpClass(cls):
        import test_otd_offline_retry as ofr   # old-vs-fix: OTGW_OTD_INO=<older OTDirect.ino>
        src = ofr.OTD_INO.read_text(encoding="utf-8").replace("\r\n", "\n")
        cls.tmp = tempfile.TemporaryDirectory()
        cpp = Path(cls.tmp.name) / "g.cpp"
        cpp.write_text(build_gap_source(src), encoding="utf-8")
        exe = compile_cpp(cpp, Path(cls.tmp.name) / "g.exe")
        if exe is None:
            raise unittest.SkipTest("no host C++ compiler")
        out = subprocess.run([str(exe)], capture_output=True, text=True, check=True).stdout.strip()
        print(out)
        _, gap, before = out.split("|")
        cls.gap, cls.before = int(gap), int(before)

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def test_no_own_frame_while_the_thermostat_is_there(self):
        self.assertEqual(self.before, 0)

    def test_the_first_own_frame_comes_within_the_spec_5_s(self):
        self.assertGreater(self.gap, 0, "the gateway must send its own frames after the loss")
        self.assertLess(self.gap, 5000, "spec v4.2 :1006-1016: under 5 s without a valid frame")


if __name__ == "__main__":
    unittest.main(verbosity=2)
