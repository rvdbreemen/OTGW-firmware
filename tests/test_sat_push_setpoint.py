#!/usr/bin/env python3
"""TASK-1272: host test for the SAT TC= push (Sergeant D entry 14, docs/sat/sergeant-d-corrections.md).

The real code is cut out of src/OTGW-firmware/SATcontrol.ino and compiled with a host C++ compiler:
the command-cache block (SAT_CMD_REFRESH_MS up to the Thermal Drop section: satResetCmdCache(),
satEnqueueIfChangedTC(), satTCNoteAdopted(), satTCPushOff()) and satFollowThermostatDecide().
Nothing is re-implemented. Not reachable from here: that satAdoptThermostatSetpoint() calls
satTCNoteAdopted() and that satControlLoopRun() calls the push helper with the else-branch satTCPushOff()
(source-level checks below). OTGW_SAT_INO points at another copy (e.g. `git show HEAD:...`) for old-vs-fix
runs. HEAD has the 5-min TC= refresh (scenario r needs the lane's satCmdResend) and pushes an adopted value back, so a, d, e, g, h, x and the wiring test
fail there.

Run: python tests/test_sat_push_setpoint.py
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

STUBS = r"""
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cmath>
#define PSTR(x) x
#define snprintf_P snprintf
struct SatState { bool bLastSentValid=false; float fLastSentCS=0; uint8_t iLastSentMM=0, iLastSentCH=0; float fLastSentTC=0;
                  bool bTCAccountedValid=false; uint32_t iTCPushMs=0; bool bPushRefused=false;
                  bool bActive=true; uint32_t iLastSentCSMs=0, iLastSentMMMs=0, iLastSentCHMs=0, iLastSentTCMs=0; };  // iLastSentTCMs: only HEAD still has it
struct State { SatState sat; };
static State state;
struct OTSys { uint8_t SlaveStatus = 0; }; static OTSys OTcurrentSystemState;   // read by satCmdResend() (TASK-1370)
static bool hasOTCommandInterface() { return true; }
static uint32_t g_ms = 0;
static uint32_t millis() { return g_ms; }
static bool isPICEnabled() { return true; }
static char g_sent[512];
static void addCommandToQueue(const char* c, size_t n, bool, int) { strncat(g_sent, c, n); strcat(g_sent, ";"); }
@BODY@
static float g_target = 20.0f;
static void tick() { satEnqueueIfChangedTC(g_target); }
static void at(uint32_t s) { g_ms = s * 1000UL; g_sent[0] = 0; tick(); if (g_sent[0]) printf("%u:%s ", s, g_sent); }
// thermostat shows `v` in two consecutive T frames: adopt through the real decide function
static float g_lastSeen = 0, g_cand = 0; static bool g_haveLast = false, g_haveCand = false;
static bool dial(float v) {
  satFollowThermostatDecide(v, true, true, state.sat.fLastSentTC, g_lastSeen, g_haveLast, g_cand, g_haveCand);
  bool r = satFollowThermostatDecide(v, true, true, state.sat.fLastSentTC, g_lastSeen, g_haveLast, g_cand, g_haveCand);
  if (r) {
    g_target = v;
#ifdef HAS_NEW
    satTCNoteAdopted(v);
#endif
  }
  return r;
}
int main(int argc, char** argv) {
  const char* sc = argv[1];
  g_ms = 100000; satResetCmdCache();
  if (sc[0] == 'a') {                      // unchanged target for 10 min, tick every 30 s
    for (uint32_t s = 100; s <= 700; s += 30) at(s);
  } else if (sc[0] == 'b') {               // target change after a quiet period
    at(100); at(130); g_target = 21.0f; at(160); at(190); at(220);
  } else if (sc[0] == 'd') {               // thermostat dial 20 -> 21.5: decide, adopt, tick
    at(100); dial(20.0f);
    printf("adopt=%d ", dial(21.5f) ? 1 : 0);
    for (uint32_t s = 110; s <= 400; s += 30) at(s);
  } else if (sc[0] == 'h') {               // dial 20 -> 21.5 -> 20: both adopted, nothing sent back
    at(100); dial(20.0f);
    int r1 = dial(21.5f) ? 1 : 0; at(110);
    int r2 = dial(20.0f) ? 1 : 0; at(140);
    printf("adopt=%d%d tgt=%.1f ", r1, r2, g_target);
    for (uint32_t s = 170; s <= 400; s += 30) at(s);
  } else if (sc[0] == 'x') {               // push 20, adopt 21.5, SAT target back to 20: TC=20 once, its echo is not adopted
    at(100); dial(20.0f);
    int r1 = dial(21.5f) ? 1 : 0; at(110);
    g_target = 20.0f; at(120); at(150);
    int r2 = dial(20.0f) ? 1 : 0; at(180); at(210);
    printf("adopt=%d echo_adopt=%d tgt=%.1f ", r1, r2, g_target);
  } else if (sc[0] == 'e') {               // push off then on again with the same target
    at(100);
#ifdef HAS_NEW
    satTCPushOff();
#endif
    at(130);
  } else if (sc[0] == 'f') {               // cache reset (offline->online) forces one resend
    at(100); at(130); satResetCmdCache(); at(160); at(190);
  } else if (sc[0] == 'r') {               // TASK-1370: the 5 s CS/MM/CH resend never carries TC=; TC= stays event-driven
    at(100); satSendCS(45.0f); satSendMM(100); satSendCH(1);
    for (uint32_t s = 101; s <= 115; s++) { g_ms = s * 1000UL; g_sent[0] = 0; satCmdResend(); tick(); if (g_sent[0]) printf("%u:%s ", s, g_sent); }
  } else if (sc[0] == 'g') {               // no re-push of any kind, whatever MsgID 9 does: 10 min
    for (uint32_t s = 100; s <= 700; s += 10) at(s);
  }
  printf("\n");
  return 0;
}
"""


def extract(src: str) -> str:
    a = src.index("static const uint32_t SAT_CMD_REFRESH_MS")
    b = src.index("// --- Thermal Drop Learning State")
    c = src.index("static bool satFollowThermostatDecide(")
    d = src.index("// Called from processOT() for every MsgID 16")
    return src[a:b] + "\n" + src[c:d]


class PushSetpoint(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.dir = tempfile.TemporaryDirectory()
        d = Path(cls.dir.name)
        src = SAT_INO.read_text(encoding="utf-8")
        cls.new = "satTCNoteAdopted" in src
        cpp = d / "t.cpp"
        cpp.write_text(("#define HAS_NEW 1\n" if cls.new else "") + STUBS.replace("@BODY@", extract(src)))
        cls.exe = compile_cpp(cpp, d / "t.exe")

    @classmethod
    def tearDownClass(cls):
        cls.dir.cleanup()

    def run_sc(self, sc):
        if not self.exe:
            self.skipTest("no host C++ compiler")
        out = subprocess.run([str(self.exe), sc], capture_output=True, text=True, check=True).stdout.strip()
        print(f"  [{sc}] {out}")
        return out

    def test_a_unchanged_target_no_repeat(self):
        out = self.run_sc("a")
        self.assertEqual(out, "100:TC=20.0;")          # HEAD: also 400:, 700: (every 5 min)

    def test_b_target_change_one_tc(self):
        out = self.run_sc("b")
        self.assertEqual(out, "100:TC=20.0; 160:TC=21.0;")

    def test_g_no_repush_ever(self):
        self.assertEqual(self.run_sc("g"), "100:TC=20.0;")      # HEAD: every 5 min

    def test_d_thermostat_adopt_no_push_back(self):
        out = self.run_sc("d")
        self.assertEqual(out, "100:TC=20.0; adopt=1")

    def test_h_dial_back_adopted_no_tc(self):
        self.assertEqual(self.run_sc("h"), "100:TC=20.0; adopt=11 tgt=20.0")

    def test_x_echo_of_own_push_not_adopted(self):
        self.assertEqual(self.run_sc("x"), "100:TC=20.0; 120:TC=20.0; adopt=1 echo_adopt=0 tgt=20.0")

    def test_e_push_off_on_resends(self):
        self.assertEqual(self.run_sc("e"), "100:TC=20.0; 130:TC=20.0;")

    def test_r_cmd_resend_is_not_a_tc_repush(self):
        # only on the lane: HEAD has no resend
        self.assertEqual(self.run_sc("r"), "100:TC=20.0; 105:CS=45.0;MM=100;CH=1; 110:CS=45.0;MM=100;CH=1; 115:CS=45.0;MM=100;CH=1;")

    def test_f_cache_reset_forces_resend(self):
        self.assertEqual(self.run_sc("f"), "100:TC=20.0; 160:TC=20.0;")


class Wiring(unittest.TestCase):
    """Source-level: what the host harness cannot reach."""
    def test_wiring(self):
        src = SAT_INO.read_text(encoding="utf-8")
        if "satTCNoteAdopted" not in src:
            self.fail("HEAD: no adopt hook")
        adopt = src[src.index("static void satAdoptThermostatSetpoint"):src.index("static bool satFollowThermostatDecide(")]
        self.assertIn("satTCNoteAdopted(", adopt)
        self.assertIn("satEnqueueIfChangedTC(settings.sat.fTargetTemp)", src)
        self.assertRegex(src, r"satEnqueueIfChangedTC\(settings\.sat\.fTargetTemp\);\s*\}\s*else\s*\{\s*satTCPushOff\(\);")
        self.assertNotIn("TrOverride", src[src.index("static bool satEnqueueIfChangedTC"):src.index("static void satTCPushOff")])


if __name__ == "__main__":
    unittest.main(verbosity=2)
