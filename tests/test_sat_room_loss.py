#!/usr/bin/env python3
"""TASK-1364: host test for "room temperature lost or missing -> heating on the curve only" (entries 38, 46).

Nothing is re-implemented. The test cuts the real code and compiles it with a host C++ compiler:
  - SATpid.ino from "// --- PID Constants ---" to the end: satPidUpdate(), its integral/derivative/gain helpers
    and (fix) satPidCurveOnly();
  - from SATcontrol.ino the room-validity block of the control tick and the `float pidOutput = ...;` statement.
    Fix: from "const bool roomValid" up to the room EMA comment, plus satRoomTempValid().
    HEAD: from "if (isnan(roomTemp) || roomTemp < -10.0f" through "_sat_consecutiveSkips = 0;" (skip counter and trip).
OTGW_SAT_INO / OTGW_SATPID_INO point at other copies (`git show HEAD:src/OTGW-firmware/SATcontrol.ino`) for the
old-vs-fix run: on HEAD the room-loss cases fail (trip after 10 invalid ticks, no curve-only output).
Not reachable from here: the rest of satControlLoopRun() (EMA, thermal block, PV boost, CS/MM/CH queueing). Those guards
are covered by source-level checks (SourceGuards). Without a host compiler the compile tests are skipped.

Run: python tests/test_sat_room_loss.py
"""
import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(Path(__file__).resolve().parent))
from test_sat_pwm_override import compile_cpp  # noqa: E402

SAT_INO = Path(os.environ.get("OTGW_SAT_INO", REPO / "src" / "OTGW-firmware" / "SATcontrol.ino"))
PID_INO = Path(os.environ.get("OTGW_SATPID_INO", REPO / "src" / "OTGW-firmware" / "SATpid.ino"))

STUBS = r"""
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#define F(x) x
#define PSTR(x) x
#define SATDebugTf(...) ((void)0)
#define SATDebugTln(x) ((void)0)
#define SAT_HSYS_UNDERFLOOR 1
struct SatSettings { float fDeadband = 0.1f; bool bAutoGains = false; float fKpManual = 2.0f, fKiManual = 1.0f, fKdManual = 100.0f;
                     float fHeatingCurveCoeff = 1.0f; bool bSolarFreezeIntegral = false; };
struct Settings { SatSettings sat; };
struct SatState { float fPidP=0, fPidI=0, fPidD=0, fPidOutput=0, fKp=0, fKi=0, fKd=0, fRawDerivative=0, fError=0;
                  bool bSolarGainActive=false, bSafetyTripped=false, bRoomTempLost=false; };
struct State { SatState sat; };
static Settings settings; static State state;
static uint32_t g_ms = 100000;
static uint32_t millis() { return g_ms; }
static void DebugTln(const char*) {}
static int g_disableCalls = 0, g_narrations = 0;
static void satDisable() { g_disableCalls++; }
static void satNarrate_P(const char*) { g_narrations++; }
static int satGetEffectiveHeatingSystem() { return 0; }
static float satGetFlowTemp() { return 40.0f; }
static const uint8_t SAT_MAX_SKIP_COUNT = 10;          // HEAD only: the real constant is not part of the cut
static uint8_t _sat_consecutiveSkips = 0;               // HEAD only
static bool _sat_roomLostActive = false;                // fix only
@PID@
@HELPER@
static void tick(float roomTemp, bool* ran, float* out) {
  const float effectiveTarget = 21.0f, curveValue = 45.0f;
  *ran = false; *out = -1.0f;
  @ROOMBLOCK@
  *ran = true;
  @PIDLINE@
  *out = pidOutput;
}
int main(int argc, char** argv) {
  // argv: room temperatures, one control tick of 30 s each ("nan" allowed)
  for (int i = 1; i < argc; i++) {
    bool ran; float out;
    g_ms += 30000;
    tick((float)atof(argv[i]), &ran, &out);
    printf("ran=%d tripped=%d disables=%d out=%.4f P=%.4f I=%.4f D=%.4f err=%.3f\n", (int)ran, (int)state.sat.bSafetyTripped,
           g_disableCalls, out, state.sat.fPidP, state.sat.fPidI, state.sat.fPidD, state.sat.fError);
  }
  return 0;
}
"""


def build_cpp(sat_src: str, pid_src: str) -> str:
    pid = pid_src[pid_src.index("// --- PID Constants ---"):]
    if "const bool roomValid" in sat_src:
        a = sat_src.index("  const bool roomValid")
        b = sat_src.index("  // TASK-894 (George)")
        roomblock = sat_src[a:b]
        h0 = sat_src.index("static bool satRoomTempValid(float roomTemp)")
        helper = sat_src[h0:sat_src.index("}\n", h0) + 2]
    else:                                   # HEAD: skip counter + trip, no roomValid
        a = sat_src.index("  if (isnan(roomTemp) || roomTemp < -10.0f")
        end = "_sat_consecutiveSkips = 0;"
        b = sat_src.index(end, a) + len(end)
        roomblock = "const bool roomValid = true; (void)roomValid;\n" + sat_src[a:b]
        helper = ""
    c = sat_src.index("float pidOutput =")
    pidline = sat_src[c:sat_src.index(";", c) + 1]
    return (STUBS.replace("@PID@", pid).replace("@HELPER@", helper)
            .replace("@ROOMBLOCK@", roomblock).replace("@PIDLINE@", pidline))


# A realistic history before the loss: temperature moves (D builds up) and sits inside the deadband (I builds up).
PRE = ["19.0"] * 3 + ["19.5"] * 3 + ["20.2"] * 3 + ["20.95", "20.96", "20.95", "20.96"]


class RoomLoss(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.dir = tempfile.TemporaryDirectory()
        d = Path(cls.dir.name)
        cpp = d / "t.cpp"
        cpp.write_text(build_cpp(SAT_INO.read_text(encoding="utf-8").replace("\r\n", "\n"),
                                 PID_INO.read_text(encoding="utf-8").replace("\r\n", "\n")),
                       encoding="utf-8")
        cls.exe = compile_cpp(cpp, d / "t.exe")

    @classmethod
    def tearDownClass(cls):
        cls.dir.cleanup()

    def run_seq(self, vals):
        if not self.exe:
            self.skipTest("no host C++ compiler")
        out = subprocess.run([str(self.exe)] + vals, capture_output=True, text=True, check=True).stdout.strip().split("\n")
        return [{k: float(v) for k, v in (kv.split("=") for kv in line.split())} for line in out]

    def test_a_no_trip_after_many_invalid_ticks(self):
        r = self.run_seq(PRE + ["nan"] * 30)
        loss = r[len(PRE):]
        self.assertEqual(sum(1 for x in loss if x["tripped"]), 0, "no trip while the room temperature is lost")
        self.assertEqual(loss[-1]["disables"], 0, "satDisable() must not run")
        self.assertTrue(all(x["ran"] == 1 for x in loss), "every tick must go on to the PID/curve step")

    def test_b_curve_only_with_p_i_d_zero(self):
        r = self.run_seq(PRE + ["nan"] * 30)
        pre = r[len(PRE) - 1]
        self.assertNotEqual(pre["I"], 0.0, "test premise: I is non-zero before the loss")
        self.assertNotEqual(pre["D"], 0.0, "test premise: D is non-zero before the loss")
        for x in r[len(PRE):]:
            self.assertAlmostEqual(x["out"], 45.0, places=3, msg="requested setpoint must be the curve value")
            self.assertEqual((x["P"], x["I"], x["D"], x["err"]), (0.0, 0.0, 0.0, 0.0))

    def test_c_out_of_range_counts_as_lost(self):
        r = self.run_seq(PRE + ["60.0", "-20.0"])
        for x in r[len(PRE):]:
            self.assertAlmostEqual(x["out"], 45.0, places=3)
            self.assertEqual((x["tripped"], x["P"], x["I"], x["D"]), (0.0, 0.0, 0.0, 0.0))

    def test_d_recovery_runs_pid_from_zero(self):
        r = self.run_seq(PRE + ["nan"] * 30 + ["20.95"])
        pre_i = r[len(PRE) - 1]["I"]
        x = r[-1]
        self.assertEqual(x["ran"], 1)
        # Ki=1, error 0.05 (inside the deadband), 60 s per update: I restarts at 0 and gains 3.0 on the first update.
        self.assertAlmostEqual(x["I"], 3.0, places=3, msg="I must restart from 0 (pre-loss I was %.3f)" % pre_i)
        self.assertEqual(x["D"], 0.0, "D must restart from 0 (inside the deadband it stays frozen at the reset value)")
        self.assertAlmostEqual(x["P"], 2.0 * 0.05, places=3)
        self.assertAlmostEqual(x["out"], 45.0 + x["P"] + x["I"] + x["D"], places=3)
        self.assertNotAlmostEqual(x["out"], 45.0, places=2, msg="the PID must have run again, not handed back the curve")

    def test_e_short_gap_recovery_is_not_swallowed(self):
        # One lost tick, then a valid temperature again within 60 s: the unchanged-error gate must not return the curve-only output.
        r = self.run_seq(PRE + ["nan", "20.96"])
        self.assertNotAlmostEqual(r[-1]["out"], 45.0, places=2)
        self.assertAlmostEqual(r[-1]["P"], 2.0 * 0.04, places=3)


class SourceGuards(unittest.TestCase):
    """The rest of satControlLoopRun() cannot be compiled on the host: check the wiring in the source."""

    @classmethod
    def setUpClass(cls):
        cls.src = SAT_INO.read_text(encoding="utf-8").replace("\r\n", "\n")

    def test_trip_machinery_for_room_loss_is_gone(self):
        self.assertNotIn("SAT_MAX_SKIP_COUNT", self.src)
        self.assertNotIn("_sat_consecutiveSkips", self.src)

    def test_pic_fail_trip_is_kept(self):
        self.assertIn("SAT_MAX_PIC_FAILS", self.src)

    def test_blocks_skipped_during_the_loss(self):
        self.assertIn("if (roomValid) {\n    static const float SAT_ROOM_FILTER_TAU_S", self.src, "room EMA")
        self.assertIn("if (roomValid && state.sat.bFallbackActive && state.sat.iLastKnownRoomMs > 0)", self.src, "MQTT takeover thermal block")
        self.assertIn("if (roomValid) {\n    satAutoTuneUpdate();", self.src, "auto-tune and statistics")
        self.assertIn("&& !roomLost) {", self.src, "PV boost start")
        self.assertIn("satPidCurveOnly(curveValue)", self.src)

    def _body(self, head):
        a = self.src.index(head)
        return self.src[a:self.src.index("\n}\n", a)]

    def test_room_temp_lost_flag_set_published_and_cleared(self):
        self.assertIn("state.sat.bRoomTempLost = !roomValid;", self.src, "set from the control tick: true during the loss, false after recovery")
        self.assertIn('publishIfChangedB(F("sat/room_temp_lost"),   state.sat.bRoomTempLost,     s_core_room_temp_lost,   true);', self.src)
        self.assertIn('je.field(F("room_temp_lost"),       snap->st.sat.bRoomTempLost);', self.src)
        for head in ("void satDisable()", "void satClearSafetyTrip()"):
            with self.subTest(head):
                b = self._body(head)
                self.assertIn("state.sat.bRoomTempLost = false;", b)
                self.assertIn("_sat_roomLostActive = false;", b)

    def test_v2_page_shows_the_hint(self):
        d = SAT_INO.parent / "data"
        self.assertIn("Room temperature lost: heating on the curve only", (d / "v2.html").read_text(encoding="utf-8"))
        self.assertIn("d.room_temp_lost", (d / "v2.js").read_text(encoding="utf-8"))

    def test_solar_gain_skips_out_of_range_like_nan(self):
        b = self._body("static void satUpdateSolarGain()")
        self.assertIn("if (!satRoomTempValid(roomTemp)) {", b)
        self.assertNotIn("if (isnan(roomTemp))", b)

    def test_stale_comments_are_gone(self):
        self.assertNotIn("skip -> safety-trip chain", self.src)
        self.assertNotIn("disengages safely", self.src)


if __name__ == "__main__":
    unittest.main(verbosity=2)
