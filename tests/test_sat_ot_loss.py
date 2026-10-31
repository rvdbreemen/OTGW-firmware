#!/usr/bin/env python3
"""TASK-1365: host test for "no valid OpenTherm -> hand the boiler back once, then send nothing" (entries 39, 47).

Nothing is re-implemented. The test cuts the real code out of SATcontrol.ino and compiles it with a host C++ compiler:
  - the SAT command cache and senders, from "static const uint32_t SAT_CMD_REFRESH_MS" up to the TC= comment
    (satResetCmdCache, satEnqueueOwnCS, satEnqueueIfChangedCS, satSendCS/MM/CH, the PIC keepalive satPicCSKeepalive and the 5 s resend satCmdResend);
  - satDisable() (the hand-back: CS=0, CH=1, MM=T);
  - fix only: the block between "// --- satOtLink begin" and "// --- satOtLink end" (satOtLinkDecide, satOtLinkStep);
  - the part of satControlLoopRun() from "If safety tripped" up to "if (!DUE(timerSATControl)) return;"
    (fix: with the satOtLinkStep() gate; HEAD: without it).
The harness runs that pass once per simulated second and, when the control timer is due, stands in for the control
tick by calling the real satSendCS/MM/CH with fixed values (45.0, 100, 1). The boiler is modelled by
stamping state.otBus.iBoilerLastSeenMs, which processOT() does for every B frame.

Not executed here: processOT() itself and the OT-Direct Data-Invalid path. A Data-Invalid reply keeps the link alive
because it is a B frame that processOT() stamps like any other; that is checked at source level (SourceGuards).
The clock-jump case is covered by construction: the decision reads millis() and iBoilerLastSeenMs, never time().

OTGW_SAT_INO points at another copy (`git show HEAD:src/OTGW-firmware/SATcontrol.ino`) for the old-vs-fix run:
on HEAD the loss cases fail (keepalive and CS/MM/CH refresh keep going, no hand-back).

Run: python tests/test_sat_ot_loss.py
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

SRC = REPO / "src" / "OTGW-firmware"
SAT_INO = Path(os.environ.get("OTGW_SAT_INO", SRC / "SATcontrol.ino"))

STUBS = r"""
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#define F(x) x
#define PSTR(x) x
#define snprintf_P snprintf
#define SATDebugTf(...) ((void)0)
// TASK-1521 log helpers
static void satNarratef_P(const char*, ...) {}
static bool _sat_followNarrPending = false;
static bool _sat_narrAtMax = false;
static bool _sat_narrCold = false;
static bool _sat_bootBoilerLogged = false;
static bool _sat_bootOutsideLogged = false;
#define SATDebugTln(x) ((void)0)
enum { SAT_MODE_OFF = 0, SAT_MODE_CONTINUOUS = 1, SAT_MODE_PWM = 2 };
struct SatSettings { bool bSimulation = false; float fTargetTemp = 20.0f; };   // fTargetTemp: TASK-1521 follow narration
struct Settings { SatSettings sat; };
struct SatState { bool bLastSentValid=false, bTCAccountedValid=false, bActive=false, bSafetyTripped=false, bFallbackActive=false,
                  bPushRefused=false, bRoomTempLost=false, bOtLinkLost=false, bOutsideTempMissing=false;
                  uint32_t iLastSentCSMs=0, iLastSentMMMs=0, iLastSentCHMs=0, iTCPushMs=0;
                  float fLastSentCS=0, fFinalSetpoint=0; uint8_t iLastSentMM=0, iLastSentCH=0; int eControlMode=0; uint32_t iSimDhwExpiryMs=0; };
struct OTSys { uint8_t SlaveStatus = 0; }; static OTSys OTcurrentSystemState;
struct OtBusState { uint32_t iBoilerLastSeenMs = 0; bool bPSmode = false; };
struct State { SatState sat; OtBusState otBus; };
static Settings settings; static State state;
static uint32_t g_ms = 1000;
static uint32_t millis() { return g_ms; }
static void DebugTln(const char*) {}
static void satNarrate_P(const char*) {}
static void addCommandToQueue(const char* cmd, int, bool, int) { printf("%u %s\n", (unsigned)(g_ms / 1000), cmd); }
static bool g_pic = true;
static bool isPICEnabled() { return g_pic; }
static bool g_iface = true;
static bool hasOTCommandInterface() { return g_iface; }
static bool _sat_bootCS0sent = true;
static bool _sat_prevFlameState = false;
static bool satIsFlameOn() { return false; }
static void satCycleOnFlameChange(bool) {}
static void satPwmNoteDhwFlameEdge(bool, bool) {}
static void satCycleSample() {}
static void satGetWindow4hStats() {}
static void satGetWindow24hStats() {}
static void satPwmSuppressionRefresh() {}
static void satMaxTSetSendStep() {}   // TASK-1373: the user MaxTSet SH= step is tested in test_sat_max_tset.py
static void satDhwSetpointSync() {}   // TASK-1376: the SW= sync is tested in test_sat_dhw_sw_sync.py
static void satMiStep() {}   // TASK-1371: MI=500 is tested in test_sat_no_return_ignition.py
static void satNoReturnFloodOnFlameEdge(bool, bool) {}
static void satNoReturnFloodStep() {}
static bool satRestartStep() { return false; }   // TASK-1482: the restart and outside gates are tested in test_sat_restart_quiet.py
static void satSavePwmEnabled(bool) {}
static void satPidService(uint32_t) {}   // TASK-1368: the 60 s PID service is not under test here
static void satSetControlMode(int m) { state.sat.eControlMode = m; }
static void satPwmReset() {}
static void satCycleSkipAutoSwitch() {}   // TASK-1367: called by satDisable(); the overshoot timer is not under test here
static float _sat_lastControlCS = 0, _sat_requestedSetpoint = 0;
static bool _sat_thermalSafeActive = false, _sat_roomLostActive = false;
// PID persistence (TASK-1365 AC#4): g_pidFile stands for the saved PID file, g_roomSeen for satPidRoomSeen().
// satPidReset() clears the room flag like the real one; satSavePidState() overwrites the file with the reset (zero) state.
static bool g_roomSeen = false; static int g_pidSaves = 0; static const char* g_pidFile = "saved";
static bool satPidRoomSeen() { return g_roomSeen; }
static bool g_ntp = true; static bool isNTPtimeSet() { return g_ntp; }   // loss_room_nontp: NTP never synced
static void satPidReset() { g_roomSeen = false; }
static void satResetControlState() {}   // TASK-1414: zone integrals, tested in test_sat_pid_resets.py
static void satSavePidState() { g_pidSaves++; g_pidFile = "zeros"; }
static void satSaveEnergyState() {}
static void satSaveEstimatedEnergy() {} static void satHCRReset() {}
struct Timer { uint32_t next; uint32_t iv; };
static Timer timerSATControl = {0, 60000}, timerSAT4hStats = {0, 60000}, timerSATPublishRetry = {0, 500};
static bool _sat_publishRetryPending = false;   // TASK-1339 retry: MQTT publish is not under test here
static void satPublishMQTT() {}
static bool due(Timer& t) { if ((int32_t)(g_ms - t.next) >= 0) { t.next = g_ms + t.iv; return true; } return false; }
#define DUE(t) due(t)
@DECL@
@SENDERS@
@DISABLE@
@LINK@
static bool prePass() {
@SPAN@
  return true;
}
// boiler schedule per scenario: does the boiler reply in second t (seconds since boot)?
static bool boiler(const char* sc, uint32_t t) {
  if (!strcmp(sc, "loss") || !strcmp(sc, "loss_room") || !strcmp(sc, "loss_room_nontp")) return t < 120;
  if (!strcmp(sc, "invalid")) return true;                     // Data-Invalid replies: B frames all the time
  if (!strcmp(sc, "resume"))  return t < 120 || t >= 420;
  if (!strcmp(sc, "sim"))     return false;
  if (!strcmp(sc, "never"))   return false;
  if (!strcmp(sc, "pic_ps"))  return t < 60 || t >= 182;      // PIC prints no B lines in PS=1; first B line 2 s after PS=0
  if (!strcmp(sc, "pic_ps_lost") || !strcmp(sc, "otd_ps") || !strcmp(sc, "wrap") || !strcmp(sc, "off_during_loss")) return t < 120;
  if (!strcmp(sc, "trip"))    return t < 100;                  // tripped 50..299 s, re-enabled at 300 s
  if (!strcmp(sc, "noiface")) return false;                    // no PIC, no OT-Direct: no B frames at all
  return true;
}
// PS=1 summary mode per scenario
static bool psmode(const char* sc, uint32_t t) {
  if (!strcmp(sc, "pic_ps"))      return t >= 60 && t < 180;
  if (!strcmp(sc, "pic_ps_lost")) return t >= 200 && t < 300;
  if (!strcmp(sc, "otd_ps"))      return t >= 60;               // OT-Direct keeps bridging B frames in PS=1
  return false;
}
int main(int argc, char** argv) {
  const char* sc = argv[1];
  uint32_t endS = (uint32_t)atoi(argv[2]);
  g_ms = (argc > 3 ? (uint32_t)atoi(argv[3]) : 1) * 1000;     // SAT's first pass (late: after a slow setup())
  settings.sat.bSimulation = !strcmp(sc, "sim");
  g_ntp = strcmp(sc, "loss_room_nontp") != 0;
  g_pic = strcmp(sc, "otd_ps") != 0;
  g_iface = strcmp(sc, "noiface") != 0;
  bool enabled = true;
  const bool late = !strcmp(sc, "late");                       // loop() drains B frames AFTER satControlLoop()
  bool jumped = false;
  for (; g_ms / 1000 < endS; g_ms += 1000) {
    // wrap: at t=200 the clock reads as if millis() wrapped 2^32 ms after the last reply (now - last = 1 s)
    if (!strcmp(sc, "wrap") && !jumped && g_ms / 1000 == 200) { g_ms = 120000; jumped = true; }
    state.otBus.bPSmode = psmode(sc, g_ms / 1000);
    const bool b = !jumped && boiler(sc, g_ms / 1000);
    if (b && !late) state.otBus.iBoilerLastSeenMs = g_ms;
    // off_during_loss: the user switches SAT off at 300 s (settingStuff.ino SATenabled=0 -> satDisable());
    // the loop's disabled branch returns before the span from then on.
    if ((!strcmp(sc, "loss_room") || !strcmp(sc, "loss_room_nontp")) && g_ms / 1000 == 10) g_roomSeen = true;   // first room reading, then the loss at 149 s
    if (!strcmp(sc, "off_during_loss") && g_ms / 1000 == 300) { satDisable(); enabled = false; }
    // trip: a safety trip at 50 s (it calls satDisable()), cleared by a re-enable at 300 s
    if (!strcmp(sc, "trip") && g_ms / 1000 == 50)  { state.sat.bSafetyTripped = true; satDisable(); }
    if (!strcmp(sc, "trip") && g_ms / 1000 == 300) state.sat.bSafetyTripped = false;
    if (enabled && prePass()) {                                           // control tick: the real senders, fixed values
      state.sat.bActive = true;
      satSendCS(45.0f);
      satSendMM(100);
      satSendCH(1);
    }
    if (b && late) state.otBus.iBoilerLastSeenMs = g_ms;
  }
  fprintf(stderr, "PIDSAVES %d file=%s\n", g_pidSaves, g_pidFile);
  printf("END lost=%d\n", (int)state.sat.bOtLinkLost);
  return 0;
}
"""


def build_cpp(src: str) -> str:
    a = src.index("static const uint32_t SAT_CMD_REFRESH_MS")
    senders = src[a:src.index("// TASK-1272, Sergeant D entry 14", a)]
    d0 = src.index("void satDisable(")
    d1 = src.index("//=== Flush short-lived", d0)
    if "// --- satOtLink begin" in src:
        d1 = min(d1, src.index("// --- satOtLink begin"))
    disable = src[d0:d1]
    decl = "void satDisable(bool persistPid = true);" if "bool persistPid" in disable else ""
    link = ""
    if "// --- satOtLink begin" in src:
        link = src[src.index("// --- satOtLink begin"):src.index("// --- satOtLink end")]
    s0 = src.index("  // If safety tripped, stay disabled until explicitly re-enabled")
    end = "  if (!DUE(timerSATControl)) return;"
    span = src[s0:src.index(end, s0) + len(end)].replace("return;", "return false;")
    return (STUBS.replace("@SENDERS@", senders).replace("@DISABLE@", disable)
            .replace("@LINK@", link).replace("@DECL@", decl).replace("@SPAN@", span))


class OtLoss(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.dir = tempfile.TemporaryDirectory()
        d = Path(cls.dir.name)
        cpp = d / "t.cpp"
        cpp.write_text(build_cpp(SAT_INO.read_text(encoding="utf-8").replace("\r\n", "\n")), encoding="utf-8")
        cls.exe = compile_cpp(cpp, d / "t.exe")

    @classmethod
    def tearDownClass(cls):
        cls.dir.cleanup()

    def run_sc(self, sc, end_s, start_s=1):
        if not self.exe:
            self.skipTest("no host C++ compiler")
        run = subprocess.run([str(self.exe), sc, str(end_s), str(start_s)], capture_output=True, text=True, check=True)
        out = run.stdout
        self.err = run.stderr.strip()
        lines = out.strip().split("\n")
        cmds = [(int(t), c) for t, c in (ln.split(" ", 1) for ln in lines[:-1])]
        return cmds, lines[-1]

    def test_a_alive_premise(self):
        cmds, _ = self.run_sc("invalid", 720)
        self.assertIn((1, "CS=45.0"), cmds, "premise: the first control tick sends CS/MM/CH")
        self.assertTrue(any(c == "CS=45.0" and t > 300 for t, c in cmds), "premise: the PIC keepalive runs")
        # TASK-1370 premise: between control ticks (60 s) the 5 s resend repeats CS/MM/CH, so test_b's silence is a real check
        self.assertEqual([c for t, c in cmds if t == 6], ["CS=45.0", "MM=100", "CH=1"], "premise: the 5 s resend runs after the tick at 1 s")

    def test_b_loss_hands_back_once_then_silence(self):
        # Last boiler reply at t=119 s. Expected: CS=0, CH=1, MM=T at t=149 s (30 s silence), then nothing up to 720 s.
        cmds, end = self.run_sc("loss", 720)
        # Inside the 30 s window SAT still runs normally (the keepalive may fire); the hand-back comes at 149 s.
        self.assertFalse([c for t, c in cmds if 120 <= t < 149 and c in ("CS=0", "MM=T")], "no hand-back before 30 s")
        after = [(t, c) for t, c in cmds if t >= 149]
        self.assertEqual(after, [(149, "CS=0"), (149, "CH=1"), (149, "MM=T")],
                         "after the last reply: one hand-back and then nothing (no keepalive, no CS/MM/CH refresh)")
        self.assertEqual(end, "END lost=1")

    def test_c_data_invalid_keeps_the_link_alive(self):
        # Data-Invalid replies are B frames: processOT() stamps them like any reply (SourceGuards).
        cmds, end = self.run_sc("invalid", 720)
        self.assertNotIn("CS=0", [c for _, c in cmds])
        self.assertNotIn("MM=T", [c for _, c in cmds])
        self.assertEqual(end, "END lost=0")

    def test_d_resume_on_first_reply(self):
        cmds, end = self.run_sc("resume", 720)
        lost = [(t, c) for t, c in cmds if 149 <= t < 420]
        self.assertEqual(lost, [(149, "CS=0"), (149, "CH=1"), (149, "MM=T")], "silence while the link is lost")
        back = [(t, c) for t, c in cmds if t >= 420]
        first = {c: t for t, c in reversed(back)}
        for c in ("CS=45.0", "MM=100", "CH=1"):
            self.assertIn(c, first, "%s re-sent after the boiler is back" % c)
            self.assertLess(first[c], 420 + 60, "%s on the first control tick after the reply" % c)
        self.assertEqual(end, "END lost=0")

    def test_e_simulation_is_exempt(self):
        cmds, end = self.run_sc("sim", 300)
        self.assertNotIn("MM=T", [c for _, c in cmds])
        self.assertEqual(end, "END lost=0")

    def test_f_boiler_never_seen_counts_from_boot(self):
        cmds, end = self.run_sc("never", 600)
        handback = [(t, c) for t, c in cmds if c in ("CS=0", "MM=T")]
        # SAT's first pass is at t=1 s: silence counts from there, so the hand-back comes at t=31 s.
        self.assertEqual(handback, [(31, "CS=0"), (31, "MM=T")], "30 s after SAT's first pass without any reply")
        self.assertFalse([1 for t, c in cmds if t > 31], "nothing after the hand-back")
        self.assertEqual(end, "END lost=1")

    def test_g_leaving_pic_ps_is_not_a_loss(self):
        # PIC PS=1 from 60 to 180 s (no B lines), PS=0 at 180 s, first B line at 182 s: leavePSMode() clears
        # bPSmode before the raw stream returns. The exempt period must not count as silence.
        cmds, end = self.run_sc("pic_ps", 400)
        self.assertFalse([(t, c) for t, c in cmds if c in ("CS=0", "MM=T")], "no hand-back after leaving PIC PS=1")
        self.assertEqual(end, "END lost=0")

    def test_h_pic_ps_during_a_loss_holds(self):
        # Lost at 149 s; PIC PS=1 from 200 to 300 s; the boiler never answers again. Only a boiler reply resumes.
        cmds, end = self.run_sc("pic_ps_lost", 400)
        self.assertEqual([(t, c) for t, c in cmds if t >= 149], [(149, "CS=0"), (149, "CH=1"), (149, "MM=T")],
                         "entering or leaving PIC PS=1 during a loss sends nothing")
        self.assertEqual(end, "END lost=1")

    def test_i_otdirect_ps_is_checked(self):
        # OT-Direct bridges every B frame in PS=1 (OTDirect.ino bridgeFrameToParser), so a silent boiler is a loss.
        cmds, end = self.run_sc("otd_ps", 300)
        self.assertEqual([(t, c) for t, c in cmds if t >= 149], [(149, "CS=0"), (149, "CH=1"), (149, "MM=T")])
        self.assertEqual(end, "END lost=1")

    def test_j_late_first_pass_is_not_a_loss(self):
        # setup() took 40 s; the first satControlLoop() runs before drainOTFrameQueue() stamps the first B frame.
        cmds, end = self.run_sc("late", 200, 40)
        self.assertFalse([(t, c) for t, c in cmds if c in ("CS=0", "MM=T")], "no hand-back on a slow boot")
        self.assertEqual(end, "END lost=0")

    def test_k_millis_wrap_does_not_resume(self):
        # Lost at 149 s; at 200 s the clock reads as if millis() wrapped (now - last reply = 1 s), no new reply.
        cmds, end = self.run_sc("wrap", 300)
        self.assertEqual([c for _, c in cmds].count("MM=T"), 1, "one hand-back, no false resume")
        seq = [c for _, c in cmds]
        self.assertEqual(seq[seq.index("MM=T") + 1:], [], "nothing after the hand-back (no CS/MM/CH, no keepalive)")
        self.assertEqual(end, "END lost=1")


    def test_l_switch_off_during_loss_sends_no_second_hand_back(self):
        # Lost at 149 s; the user switches SAT off at 300 s. The hand-back already went: nothing is sent again.
        cmds, end = self.run_sc("off_during_loss", 400)
        self.assertEqual([(t, c) for t, c in cmds if t >= 149], [(149, "CS=0"), (149, "CH=1"), (149, "MM=T")],
                         "exactly one CS=0, CH=1, MM=T for one loss")
        self.assertEqual(end, "END lost=0", "the switch-off ends the loss state")

    def test_m_trip_is_not_silence(self):
        # Trip at 50 s (its own hand-back), boiler silent from 100 s, re-enable at 300 s. Like a switch-off, the
        # tripped time does not count: 30 s of silence after the re-enable, hand-back at 329 s, not at 300 s.
        cmds, end = self.run_sc("trip", 400)
        self.assertEqual([(t, c) for t, c in cmds if c in ("CS=0", "MM=T")],
                         [(50, "CS=0"), (50, "MM=T"), (329, "CS=0"), (329, "MM=T")])
        self.assertEqual(end, "END lost=1")

    def test_n_no_interface_leaves_the_trip_in_charge(self):
        # No PIC and not OT-Direct: no B frames can arrive. The loss check stays out of the way, so the control
        # tick runs and the existing 5-tick interface trip (SAT_MAX_PIC_FAILS, outside this span) still latches.
        cmds, end = self.run_sc("noiface", 300)
        self.assertNotIn("MM=T", [c for _, c in cmds], "no OpenTherm-loss hand-back without a command interface")
        self.assertEqual(end, "END lost=0")

    def test_o_loss_before_first_room_reading_keeps_the_saved_pid(self):
        # Entry 115, TASK-1365 AC#4: a boiler that never answers is lost 30 s after SAT's first pass, before any room
        # reading. The hand-back must not overwrite the saved PID file with zeros (the NTP-gated restore may still be
        # pending, or it has run and the restored state must stay on flash). The restore itself is not executed here.
        cmds, end = self.run_sc("never", 600)
        self.assertEqual([(t, c) for t, c in cmds if t >= 31], [(31, "CS=0"), (31, "CH=1"), (31, "MM=T")], "hand-back still goes out")
        self.assertEqual(self.err, "PIDSAVES 0 file=saved", "the saved PID file is not overwritten")
        self.assertEqual(end, "END lost=1")

    def test_p_loss_after_room_reading_persists_zeros(self):
        # Existing behaviour kept: with a room reading seen, the hand-back persists the reset PID.
        cmds, end = self.run_sc("loss_room", 720)
        self.assertEqual([(t, c) for t, c in cmds if t >= 149], [(149, "CS=0"), (149, "CH=1"), (149, "MM=T")])
        self.assertEqual(self.err, "PIDSAVES 1 file=zeros", "zeros persisted once")
        self.assertEqual(end, "END lost=1")

    def test_q_loss_with_room_reading_but_no_ntp_keeps_the_saved_pid(self):
        # Review finding: a room reading arrives before NTP, then the loss. Zeros stamped with an unsynced time would
        # make the NTP-gated satLoadPidState() discard the file, so the hand-back must not write it either.
        cmds, end = self.run_sc("loss_room_nontp", 720)
        self.assertEqual([(t, c) for t, c in cmds if t >= 149], [(149, "CS=0"), (149, "CH=1"), (149, "MM=T")])
        self.assertEqual(self.err, "PIDSAVES 0 file=saved", "no PID write before NTP time")
        self.assertEqual(end, "END lost=1")


class SourceGuards(unittest.TestCase):
    """What the harness cannot execute: the wiring in satControlLoopRun(), processOT() and OT-Direct."""

    @classmethod
    def setUpClass(cls):
        cls.sat = SAT_INO.read_text(encoding="utf-8").replace("\r\n", "\n")
        cls.core = (SRC / "OTGW-Core.ino").read_text(encoding="utf-8").replace("\r\n", "\n")
        cls.otd = (SRC / "OTDirect.ino").read_text(encoding="utf-8").replace("\r\n", "\n")

    def _body(self, head):
        a = self.sat.index(head)
        return self.sat[a:self.sat.index("\n}\n", a)]

    def test_gate_precedes_every_sender(self):
        run = self._body("static void satControlLoopRun()\n{")
        g = run.index("if (satOtLinkStep()) return;")
        self.assertGreater(g, run.index("if (state.sat.bSafetyTripped) return;"))
        boot = ("if (satRestartStep()) return;" if "if (satRestartStep()) return;" in run
                else "if (!_sat_bootCS0sent && hasOTCommandInterface())")   # TASK-1482 replaced the boot CS=0
        for later in (boot, "  satPicCSKeepalive();",
                      "  satPwmSuppressionRefresh();", "if (!DUE(timerSATControl)) return;"):
            with self.subTest(later):
                self.assertLess(g, run.index(later))

    def test_disable_and_flash_rebaseline(self):
        run = self._body("static void satControlLoopRun()\n{")
        self.assertIn("_sat_otLinkBaseMs = millis();", run)
        self.assertIn("if (!satIsEnabled()) state.sat.bOtLinkLost = false;", run)
        self.assertIn("state.sat.bOtLinkLost = false;", self._body("void satDisable(bool persistPid)"))
        self.assertIn("if (state.sat.bSafetyTripped) _sat_otLinkBaseMs = millis();", run)

    def test_decision_uses_millis_not_time(self):
        a = self.sat.index("// --- satOtLink begin")
        blk = self.sat[a:self.sat.index("// --- satOtLink end")]
        self.assertIn("state.otBus.iBoilerLastSeenMs", blk)
        self.assertNotIn("time(", blk.replace("time() jumps", ""))
        self.assertIn("satDisable(persistPid);", blk, "the hand-back is satDisable()'s own CS=0, CH=1, MM=T")
        self.assertIn("const bool persistPid = satPidRoomSeen() && isNTPtimeSet();", blk, "read before satDisable() resets the flag")

    def test_every_b_frame_is_stamped(self):
        b = self.core.index("if (buf[0]=='B'){")
        stamp = self.core.index("const uint32_t boilerSeenMs = millis();", b)
        self.assertIn("state.otBus.iBoilerLastSeenMs = (boilerSeenMs != 0) ? boilerSeenMs : 1;",
                      self.core[stamp:stamp + 200], "one store, never 0")
        self.assertLess(stamp, self.core.index("if (!localAnswer && !replayed)", b),
                        "stamped for every B frame, not only real-boiler frames")
        self.assertNotRegex(self.core[self.core.index("bool isvalidotmsg"):][:400], r"buf\[[3-8]\]",
                            "isvalidotmsg does not filter on the message type, so a Data-Invalid B line passes")

    def test_otdirect_bridges_data_invalid_as_b(self):
        a = self.otd.index("} else if (boilerDataInvalid) {")
        self.assertIn("bridgeFrameToParser('B', response);", self.otd[a:a + 200])

    def test_status_mqtt_and_v2(self):
        self.assertIn('publishIfChangedB(F("sat/ot_link_lost"),     state.sat.bOtLinkLost,       s_core_ot_link_lost,     true);', self.sat)
        self.assertIn("memset(&s_core_ot_link_lost, 0, sizeof(s_core_ot_link_lost));", self.sat)
        self.assertIn('je.field(F("ot_link_lost"),         snap->st.sat.bOtLinkLost);', self.sat)
        d = SRC / "data"
        self.assertIn("OpenTherm lost: boiler handed back", (d / "v2.html").read_text(encoding="utf-8"))
        self.assertIn("d.ot_link_lost", (d / "v2.js").read_text(encoding="utf-8"))

    def test_no_enqueue_in_processot(self):
        a = self.core.index("void processOT(const char *buf, int len")
        self.assertNotIn("satOtLink", self.core[a:a + 20000])


if __name__ == "__main__":
    unittest.main(verbosity=2)
