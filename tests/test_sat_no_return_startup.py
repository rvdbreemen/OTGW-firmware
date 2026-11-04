#!/usr/bin/env python3
"""TASK-1412.12 (LL-018): the missing-return start-up contract, from the boiler frame to the command on the wire, on both transports.

tests/test_sat_no_return_ignition.py drives the PWM ignition with STUBBED return/flow validity (satReturnFresh, satGetFlowTemp ...).
tests/test_sat_flow_return_validity.py drives the REAL validity with no ignition. This module joins them: nothing is re-implemented,
the host program is cut out of the production sources:
  - SATboilerFeed.h as it is, the processOT() frame hook of OTGW-Core.ino (stamps the feeds from boiler Read-Ack / Data-Invalid /
    Unknown-DataID frames),
  - the SATcontrol.ino command cache block, the PWM block (satPwmFlameOffHold, satApplyPWM, satPwmSuppressionRefresh ...), the feed
    validity block (satReturnFresh, satReturnMissingLong, satGetFlowTemp, satGetReturnTemp, satNoReturnTemp) and the flood / MI=500
    block (satNoReturnFloodOnFlameEdge, satNoReturnFloodStep, satMiStep).
The harness drives them in the order of satControlLoopRun() and models the boiler as frames: a thermostat poll of MsgID 25 every
`flowper` ms, a PM=25 answered `pmdelay` ms after the command, an optional MsgID 28 poll. The device is a model, not a measurement: the
sample ages printed here are the model's, the real ones are for the bench (TASK-1412.21).

OT-Direct (class OtDirectPm): the PM= branch of the OT-Direct command handler and scheduleMasterRequest() are cut out of OTDirect.ino
and driven the same way, so the OT-Direct path of PM=25 is proven on its own code, not by the PIC queue. What a host test cannot
close: a replay or a command echo is not physical ignition. That stays a bench item (TASK-1412.21).

Run: python tests/test_sat_no_return_startup.py
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
import test_sat_no_return_ignition as nri  # noqa: E402

FW = REPO / "src" / "OTGW-firmware"
SAT_INO = Path(os.environ.get("OTGW_SAT_INO", FW / "SATcontrol.ino"))
CORE_INO = Path(os.environ.get("OTGW_CORE_INO", FW / "OTGW-Core.ino"))
FEED_H = Path(os.environ.get("OTGW_FEED_H", FW / "SATboilerFeed.h"))
OTD_INO = Path(os.environ.get("OTGW_OTD_INO", FW / "OTDirect.ino"))


def _read(p):
    return Path(p).read_text(encoding="utf-8").replace("\r\n", "\n")


def _stubs():
    """The ignition harness stubs with the validity stubs replaced by the real feed block's inputs."""
    s = nri.STUBS
    # state / settings the real feed block reads
    s = s.replace("struct State { SatState sat; };",
                  "struct OTBusStub { SatBoilerFeed feedTboiler; SatBoilerFeed feedTret; };\nstruct State { SatState sat; OTBusStub otBus; };")
    s = s.replace("struct OTSys { int SlaveStatus = 0; };", "struct OTSys { int SlaveStatus = 0; float Tboiler = 0.0f, Tret = 0.0f; };")
    s = s.replace("struct SatState { int iCurrentModulation=0;", "struct SatState { float fSimFlowTemp=0, fSimReturnTemp=0; int iCurrentModulation=0;")
    drop = ["static float satGetFlowTemp() { return g_flow; }\n", "static float satGetReturnTemp() { return g_ret; }\n",
            "static bool isBoilerMsgIdUnsupportedRead(uint8_t id) { return id == 28 && g_noReturn; }\n",
            "static bool picBoardTretAbsent() { return !g_boardTret; }\n",
            "static bool picBoardTretValid() { return g_boardTret; }                // only the version before the review fixes calls it            // settled: no board sensor reading\n"]
    for d in drop:
        assert d in s, d
        s = s.replace(d, "")
    s = re.sub(r"// TASK-1412\.5: satNoReturnTemp\(\) now asks.*?static bool satReturnMissingLong\(\) \{ return false; \}\n", "", s, flags=re.S)
    assert "satReturnFresh" not in s and "satReturnMissingLong" not in s
    extra = r"""
static float satSimNoiseOffset() { return 0.0f; }
static bool g_unsup25 = false, g_unsup28 = false, g_boardValid = false, g_boardAbsent = true;
static bool isBoilerMsgIdUnsupportedRead(uint8_t id) { return id == 28 ? g_unsup28 : (id == 25 ? g_unsup25 : false); }
static bool picBoardTretValid() { return g_boardValid; }     // the PR=E board sensor poll result (its own tests: test_sat_pic_board_tret)
static bool picBoardTretAbsent() { return g_boardAbsent; }
static float satGetFlowTemp();
static float satGetReturnTemp();
static bool satNoReturnTemp();
enum { OT_READ_ACK = 4, OT_DATA_INVALID = 6, OT_UNKNOWN_DATA_ID = 7, OT_Tboiler = 25, OT_Tret = 28 };
enum { OTGW_BOILER = 1 };
struct OTd { int id = 0, type = 0, masterslave = 0, rsptype = 0; bool skipthis = false, bLocalAnswer = false, bReplayed = false; };
static OTd OTdata;
"""
    return s + extra


MAIN = r"""
static bool g_prevFlame = false, g_enabled = true;
static uint32_t g_nextFlow = 0, g_flowPer = 0, g_nextRet = 0, g_retPer = 0, g_pmAt = 0, g_pmDelay = 300, g_nextTick = 0;
static float g_flowVal = 40, g_retVal = 30, g_slopeUp = 0.5f, g_slopeDown = 0.0f;
static int g_pmDue = 0;

static void frame(int id, int type, float v) {    // a boiler reply as processOT() sees it: the stored value, then the feed hook
  OTdata = OTd(); OTdata.id = id; OTdata.type = type; OTdata.masterslave = 1; OTdata.rsptype = OTGW_BOILER;
  if (type == OT_READ_ACK) { if (id == 25) OTcurrentSystemState.Tboiler = v; if (id == 28) OTcurrentSystemState.Tret = v; }
  FRAME_HOOK
}
static void flowFrame() { frame(25, OT_READ_ACK, g_flowVal); }

// the model of the boiler's flow temperature: rises while the flame burns, falls (or not) while it is off
static void modelStep() {
  g_flowVal += (g_flame ? g_slopeUp : -g_slopeDown) * 0.1f;
}

static void loopPass() {   // order of satControlLoopRun(); the frames arrive whatever SAT does
  modelStep();
  const uint32_t now = g_ms - g_base;
  if (g_flowPer && now >= g_nextFlow) { flowFrame(); g_nextFlow += g_flowPer; }
  if (g_retPer && now >= g_nextRet) { frame(28, OT_READ_ACK, g_retVal); g_nextRet += g_retPer; }
  if (g_pmDue && now >= g_pmAt) { flowFrame(); g_pmDue = 0; }
  if (!g_enabled) return;                       // satControlLoopRun() returns before the senders while SAT is off
  const bool f = satIsFlameOn();
  if (f != g_prevFlame) {
    const bool dhw = (OTcurrentSystemState.SlaveStatus & 0x04) != 0;
    satNoReturnFloodOnFlameEdge(f, dhw);
    g_prevFlame = f;
  }
  satMiStep();
  satNoReturnFloodStep();
  g_due = ((g_ms - g_base) % 3000UL) == 0;
  satPwmSuppressionRefresh();
  satCmdResend();
}
static void tick(float pid) {   // the PWM part of the 30 s control tick
  state.sat.fPidOutput = pid; _sat_requestedSetpoint = pid;
  float cs = satApplyPWM(pid);
  if (cs > g_effMax) cs = g_effMax;
  state.sat.fFinalSetpoint = cs; _sat_lastControlCS = cs;
  satSendCS(cs); satSendMM(80); satSendCH(1);
  printf("# tick %lu cs=%.1f\n", (unsigned long)(g_ms - g_base), cs);
}
static void run(uint32_t untilMs) { while (g_ms - g_base < untilMs) { g_ms += 100; loopPass(); } }
static void init() {
  settings = Settings(); state = State(); OTcurrentSystemState = OTSys();
  g_ms = g_base; g_flame = false; g_flow = 40; g_ret = 0; g_prevFlame = false; g_hasOT = true; g_pic = true; g_noReturn = true;
  g_boardTret = false; g_confirmed = true; g_pmBusy = false; g_quirks = 0; g_cold = 28.2f; g_effMax = 62.0f; g_userMax = 62.0f; g_flameOnStart = g_ms;
  g_unsup25 = g_unsup28 = false; g_boardValid = false; g_boardAbsent = true; g_enabled = true;
  state.sat.bPwmFlameRequested = true; state.sat.fFinalSetpoint = 50.0f; _sat_lastControlCS = 50.0f; _sat_requestedSetpoint = 55.0f;
  satResetCmdCache();
}
static void q() {
  const char* fc = "?";
  printf("Q t=%lu nr=%d fresh=%d ret=%.1f flow=%.1f phase_on=%d mode=%d final=%.1f\n", (unsigned long)(g_ms - g_base), (int)satNoReturnTemp(),
         (int)satReturnFresh(), satGetReturnTemp(), satGetFlowTemp(), (int)state.sat.bPwmFlameRequested, state.sat.eControlMode, state.sat.fFinalSetpoint);
  (void)fc;
}
int main() {
  init();
  char line[256];
  while (fgets(line, sizeof line, stdin)) {
    char cmd[32] = {0}, a[32] = {0}; double v = 0;
    int n = sscanf(line, "%31s %31s %lf", cmd, a, &v);
    if (n < 1) continue;
    if (!strcmp(cmd, "base")) { g_base = (uint32_t)atoi(a); g_ms = g_base; g_flameOnStart = g_ms; g_nextFlow = g_nextRet = 0; }
    else if (!strcmp(cmd, "run")) run((uint32_t)atoi(a));
    else if (!strcmp(cmd, "tick")) tick((float)atof(a));
    else if (!strcmp(cmd, "q")) q();
    else if (!strcmp(cmd, "flame")) { g_flame = atoi(a) != 0; if (g_flame) g_flameOnStart = g_ms + 100; else g_flameOffStart = g_ms; }
    else if (!strcmp(cmd, "enabled")) g_enabled = atoi(a) != 0;
    else if (!strcmp(cmd, "mode")) { state.sat.eControlMode = atoi(a); state.sat.bPwmFlameRequested = atoi(a) == SAT_MODE_PWM; }
    else if (!strcmp(cmd, "pic")) g_pic = atoi(a) != 0;
    else if (!strcmp(cmd, "flow")) g_flowVal = (float)atof(a);
    else if (!strcmp(cmd, "flowper")) { g_flowPer = (uint32_t)atoi(a); g_nextFlow = g_ms - g_base; }
    else if (!strcmp(cmd, "slopeup")) g_slopeUp = (float)atof(a);
    else if (!strcmp(cmd, "slopedown")) g_slopeDown = (float)atof(a);
    else if (!strcmp(cmd, "pmdelay")) g_pmDelay = (uint32_t)atoi(a);
    else if (!strcmp(cmd, "retper")) { g_retPer = (uint32_t)atoi(a); g_retVal = (float)v; g_nextRet = g_ms - g_base; }
    else if (!strcmp(cmd, "ret")) { const int t = !strcmp(a, "ack") ? OT_READ_ACK : (!strcmp(a, "inv") ? OT_DATA_INVALID : OT_UNKNOWN_DATA_ID); frame(28, t, (float)v); }
    else if (!strcmp(cmd, "unsup28")) g_unsup28 = atoi(a) != 0;
    else if (!strcmp(cmd, "unsup25")) g_unsup25 = atoi(a) != 0;
    else if (!strcmp(cmd, "confirmed")) g_confirmed = atoi(a) != 0;
    else if (!strcmp(cmd, "board")) { g_boardValid = atoi(a) != 0; g_boardAbsent = !g_boardValid; }
    else if (!strcmp(cmd, "boardunknown")) g_boardAbsent = false;
    else if (!strcmp(cmd, "pmbusy")) g_pmBusy = atoi(a) != 0;
    else if (!strcmp(cmd, "dhw")) OTcurrentSystemState.SlaveStatus = atoi(a) ? 0x04 : 0;
    else if (!strcmp(cmd, "quirk")) g_quirks = atoi(a) ? SAT_QUIRK_MI_500_BOOT : 0;
    else return 2;
  }
  return 0;
}
"""

# addCommandToQueue stub of the ignition harness prints "T cmd"; here it also tells the model a PM=25 was asked, and prints the age of the flow sample
STUB_QUEUE_OLD = 'static CmdQueueResult addCommandToQueue(const char* c, size_t n, bool, int, bool = false) { printf("%lu %.*s\\n", (unsigned long)(g_ms - 1000000UL), (int)n, c); return CmdQueueResult::Queued; }'
STUB_QUEUE_NEW = r"""static uint32_t g_pmAt, g_pmDelay; static int g_pmDue;
static CmdQueueResult addCommandToQueue(const char* c, size_t n, bool, int, bool = false) {
  printf("%lu %.*s\n", (unsigned long)(g_ms - g_base), (int)n, c);
  if (n >= 5 && !strncmp(c, "PM=25", 5)) { g_pmDue = 1; g_pmAt = (g_ms - g_base) + g_pmDelay; }   // the boiler answers MsgID 25 after pmdelay ms
  if (n >= 3 && !strncmp(c, "CS=", 3)) printf("@ %lu age=%lu\n", (unsigned long)(g_ms - g_base), (unsigned long)(state.otBus.feedTboiler.sampleMs ? g_ms - state.otBus.feedTboiler.sampleMs : 0xFFFFFFFFUL));
  return CmdQueueResult::Queued; }"""


def build_source():
    src = _read(SAT_INO)
    core = _read(CORE_INO)
    feed = _read(FEED_H).replace("#pragma once", "")
    cmd = nri._cut(src, "static const uint32_t SAT_CMD_REFRESH_MS", "// --- Thermal Drop Learning State")
    pwm = nri._cut(src, "static float    _pwm_effectiveBoilerTemp", "// Last PWM off-phase duration in ms")
    marker = "// Flow (MsgID 25) and return (MsgID 28) feed validity"
    a = src.index(marker)
    validity = src[a:src.index("static bool satIsFlameOn()", a)]
    f0 = src.index("static bool     _flood_active")
    mi = src.index("static void satMiStep()")
    flood = src[f0:src.index("\n}\n", mi) + 3]
    hs = core.index("if ((OTdata.id == OT_Tboiler || OTdata.id == OT_Tret)")
    he = core.index("\n      }\n", hs) + len("\n      }\n")
    hook = core[hs:he]
    stubs = _stubs()
    assert STUB_QUEUE_OLD in stubs
    stubs = stubs.replace(STUB_QUEUE_OLD, STUB_QUEUE_NEW)
    main = MAIN.replace("FRAME_HOOK", hook)
    # the ignition stubs declared g_pmAt etc. nowhere else; main redeclares none of them
    main = main.replace("static uint32_t g_nextFlow = 0, g_flowPer = 0, g_nextRet = 0, g_retPer = 0, g_pmAt = 0, g_pmDelay = 300, g_nextTick = 0;",
                        "static uint32_t g_nextFlow = 0, g_flowPer = 0, g_nextRet = 0, g_retPer = 0, g_nextTick = 0;").replace("static int g_pmDue = 0;\n", "")
    stubs = stubs.replace("static uint32_t g_pmAt, g_pmDelay; static int g_pmDue;", "static uint32_t g_pmAt = 0, g_pmDelay = 300, g_base = 1000000; static int g_pmDue = 0;")
    return feed + "\n" + stubs + "\nstatic void satPwmReset_decl_unused() {}\n" + cmd + "\n" + pwm + "\n" + validity + "\n" + flood + "\n" + main


class NoReturnStartup(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.dir = tempfile.TemporaryDirectory()
        d = Path(cls.dir.name)
        cpp = d / "t.cpp"
        cpp.write_text(build_source(), encoding="utf-8")
        cls.exe = compile_cpp(cpp, d / "t.exe")

    @classmethod
    def tearDownClass(cls):
        cls.dir.cleanup()

    def sim(self, script):
        """Run the script (one command per line). Returns (commands [(t, cmd)], ages {t: ms}, queries [dict], ticks [(t, cs)])."""
        if not self.exe:
            self.skipTest("no host C++ compiler")
        out = subprocess.run([str(self.exe)], input="\n".join(script) + "\n", capture_output=True, text=True, check=True).stdout.splitlines()
        cmds, ages, qs, ticks = [], {}, [], []
        for l in out:
            if l.startswith("@"):
                _, t, age = l.split()
                ages[int(t)] = int(age.split("=")[1])
            elif l.startswith("Q "):
                qs.append(dict(p.split("=") for p in l.split()[1:]))
            elif l.startswith("# tick"):
                p = l.split()
                ticks.append((int(p[2]), float(p[3].split("=")[1])))
            elif l.startswith("#"):
                continue
            else:
                t, c = l.split(" ", 1)
                cmds.append((int(t), c))
        return cmds, ages, qs, ticks

    @staticmethod
    def css(cmds, a, b):
        return [(t, c) for t, c in cmds if a <= t < b and c.startswith("CS=")]

    @staticmethod
    def pms(cmds, a=0, b=10**9):
        return [t for t, c in cmds if a <= t < b and c == "PM=25"]

    # a boiler that is ignited: pre-flame tick, run to 4 s, flame, run 60 s. `pre` are the lines that set the return facts.
    def ignite(self, pre, flame_at=4000, pic=1, end=40000):
        return pre + ["pic %d" % pic, "flowper 5000", "flow 40", "slopeup 0.5", "run 100", "tick 55", "run %d" % flame_at, "flame 1", "run %d" % end, "q"]

    # ---- the three kinds of "no return temperature" the contract names, through the REAL validity ---------------------------
    def test_unsupported_return_floods_from_the_first_ignition(self):
        # MsgID 28 answered Unknown-DataID by a boiler frame of this boot, no board sensor (entries 84, 135): no return at once.
        cmds, ages, qs, ticks = self.sim(self.ignite(["ret unk 0", "unsup28 1"]))
        self.assertEqual(qs[-1]["nr"], "1")
        self.assertEqual(ticks[0][1], 58.0)                                # pre-flame hold: flow 40 + 18 (entry 149), not return(0) + 18
        flood = [(t, c) for t, c in cmds if 4100 <= t < 24100 and (c.startswith("CS=") or c == "PM=25")]
        self.assertEqual([t for t, _ in flood], list(range(4100, 24100, 1000)))
        self.assertEqual([c == "PM=25" for _, c in flood], [i % 2 == 1 for i in range(20)])
        self.assertEqual(flood[0][1], "CS=58.0")                           # entry 151: CS = flow + 18 at t = 0
        self.assertFalse(self.pms(cmds, 24100))                            # entry 149: after the 20 s the suppression, no PM=25

    def test_data_invalid_return_waits_5_minutes_then_counts_as_no_return(self):
        # entry 217: a return invalid or absent for 5 minutes counts as no return temperature. A valid sample at t=0, then Data-Invalid.
        pre = ["retper 5000 30", "run 1000", "retper 0 0", "ret inv 0"]
        # inside the 5 minutes: thermo-nova's hold return + 18 = 48, no flood
        cmds, _, qs, ticks = self.sim(pre + ["pic 1", "flowper 5000", "flow 40", "run 200000", "tick 55", "run 204000", "flame 1", "run 230000", "q"])
        self.assertEqual(qs[-1]["nr"], "0")
        self.assertEqual(ticks[0][1], 48.0)
        self.assertEqual(self.pms(cmds), [])
        # after the 5 minutes since the last valid sample: no return temperature, flow + 18 and the flood
        cmds, _, qs, ticks = self.sim(pre + ["pic 1", "flowper 5000", "flow 40", "run 302000", "tick 55", "run 306000", "flame 1", "run 340000", "q"])
        self.assertEqual(qs[-1]["nr"], "1")
        self.assertEqual(ticks[0][1], 58.0)
        self.assertEqual(len(self.pms(cmds, 306100, 326100)), 10)

    def test_start_up_window_keeps_the_previous_cs_until_5_minutes_without_a_return(self):
        # Entry 263 (thermo-nova heating_control.py:392-395): no MsgID 28 answer of any kind this boot. Inside the first 5 minutes the
        # flame-off hold keeps the previous CS (50.0 in the harness); from 5 minutes on the boiler counts as a no-return boiler
        # (entry 217) and the flow-based hold flow + 18 applies again. Red on the old code: 28.2 (0 + 18 floored to the cold setpoint).
        pre = ["base 1000", "pic 1", "flowper 5000", "flow 40"]   # base 1000: millis() since boot is 0..300 s, no sample ever
        cmds, _, qs, ticks = self.sim(pre + ["run 100000", "tick 55", "q"])
        self.assertEqual(qs[-1]["nr"], "0")
        self.assertEqual(ticks[0][1], 50.0)
        cmds, _, qs, ticks = self.sim(pre + ["run 302000", "tick 55", "q"])
        self.assertEqual(qs[-1]["nr"], "1")
        self.assertEqual(ticks[0][1], 58.0)

    def test_stale_return_is_the_same_as_an_invalid_one_and_recovers(self):
        # no MsgID 28 frame at all after t=1 s (a thermostat that stopped asking): stale after 5 minutes (entry 217)
        pre = ["retper 5000 30", "run 1000", "retper 0 0", "pic 1", "flowper 5000", "flow 40"]
        cmds, _, qs, ticks = self.sim(pre + ["run 299000", "q", "run 302000", "q"])
        self.assertEqual([q["nr"] for q in qs], ["0", "1"])                # 299 s: still a return; 302 s: no return
        # a valid sample restores it: nr back to 0 and the hold goes back to return + 18
        cmds, _, qs, ticks = self.sim(pre + ["run 302000", "q", "ret ack 31", "q", "tick 55"])
        self.assertEqual([q["nr"] for q in qs], ["1", "0"])
        self.assertEqual(ticks[0][1], 49.0)

    def test_fresh_zero_return_is_a_measurement_not_a_missing_return(self):
        # LL-018: a measured 0.0 C is not the same as no sensor. satNoReturnTemp() is false; satGetReturnTemp() reads the sample.
        cmds, _, qs, ticks = self.sim(["pic 1", "flowper 5000", "flow 40", "ret ack 0", "run 2000", "q"])
        self.assertEqual(qs[-1]["nr"], "0")
        self.assertEqual(qs[-1]["fresh"], "1")

    def test_board_sensor_return_is_a_return_even_when_the_boiler_says_unknown(self):
        # entry 135: Unknown-DataID on 28 and a valid 'R' board reading: thermo-nova's hold, no flood, no MI=500
        pre = ["ret unk 0", "unsup28 1", "board 1", "retper 0 0"]
        cmds, _, qs, ticks = self.sim(self.ignite(pre) + ["run 400000"])
        self.assertEqual(qs[0]["nr"], "0")
        self.assertEqual(self.pms(cmds), [])
        self.assertEqual([c for _, c in cmds if c.startswith("MI=")], [])

    def test_unsupported_verdict_restored_from_file_is_not_enough_alone(self):
        # A boiler frame of this boot must confirm the verdict (our choice). In that boot window nothing floods and the hold has no
        # return temperature to use. Entry 263 (maintainer, 2026-11-04; thermo-nova heating_control.py:392-395): the hold keeps the
        # previous CS (50.0 in the harness) instead of the old 0 + 18 floored to the cold setpoint 28.2, which hit every boiler.
        # Red on the code before the fix (28.2), green after.
        cmds, _, qs, ticks = self.sim(self.ignite(["base 1000", "unsup28 1", "confirmed 0", "boardunknown 1"]))
        self.assertEqual(qs[-1]["nr"], "0")
        self.assertEqual(ticks[0][1], 50.0)
        self.assertEqual(self.pms(cmds), [])

    # ---- MsgID 25 missing: no flood on a flow that is not there ----------------------------------------------------------
    def test_missing_flow_sends_no_flood_and_no_flow_based_hold(self):
        cmds, _, qs, ticks = self.sim(["pic 1", "unsup28 1", "confirmed 1", "run 1000", "tick 55", "flame 1", "run 30000", "q"])
        self.assertEqual(qs[-1]["flow"], "0.0")                            # never received: the missing sentinel, not 0 C as a measurement
        self.assertEqual(self.pms(cmds), [])                               # the flood needs a flow temperature (satNoReturnFloodAllowed)

    # ---- sample age of the flow at each flood CS (entry 23 / R39) --------------------------------------------------------
    def test_flood_cs_uses_a_flow_sample_at_most_one_second_old_when_the_boiler_answers_pm25(self):
        # model: the boiler answers PM=25 300 ms after the command (OpenTherm v4.2 section 4.3.1 table: slave answering time 20..400 ms)
        cmds, ages, qs, ticks = self.sim(self.ignite(["ret unk 0", "unsup28 1"]) )
        flood_cs = [t for t, c in self.css(cmds, 4100, 24100)]
        self.assertEqual(len(flood_cs), 10)
        # the first CS at t = 0 has the thermostat's last poll (up to flowper = 5 s old); from the second one the PM=25 answer is in
        later = [ages[t] for t in flood_cs[1:]]
        self.assertTrue(all(a <= 1100 for a in later), later)             # 1000 ms between PM=25 and the next CS, plus the 100 ms model step
        self.assertLessEqual(max(later), 1100)

    def test_without_an_answer_to_pm25_the_flood_uses_the_thermostat_poll_age(self):
        # model: the boiler never answers PM=25 within the flood (pmdelay 60 s), the thermostat polls MsgID 25 every 5 s: the CS
        # values follow a sample up to 5 s old. This is the case the 3 s timerSATSuppress had been blamed for (R39).
        cmds, ages, qs, ticks = self.sim(["pmdelay 60000"] + self.ignite(["ret unk 0", "unsup28 1"]))
        flood_cs = [t for t, c in self.css(cmds, 4100, 24100)]
        a = [ages[t] for t in flood_cs]
        self.assertLessEqual(max(a), 5100)
        self.assertGreater(max(a), 2000)                                   # shows that without the answer the flood runs on old samples

    # ---- runtime enable ---------------------------------------------------------------------------------------------------
    def test_runtime_enable_with_a_running_boiler_sends_no_flood_and_defers_mi500_5_minutes_on_pic(self):
        # SAT off, boiler burning with an Unknown-DataID return. SAT is switched on at 60 s in continuous mode (satControlLoopRun sets
        # SAT_MODE_CONTINUOUS on the first tick, SATcontrol.ino:6818): no PWM ON phase, so no flood. MI=500 follows 5 min after the enable.
        script = ["pic 1", "flowper 5000", "flow 50", "enabled 0", "mode 1", "ret unk 0", "unsup28 1", "flame 1", "run 60000", "enabled 1", "run 359000", "q",
                  "run 361000"]
        cmds, _, qs, ticks = self.sim(script)
        self.assertEqual(self.pms(cmds), [])
        mi = [(t, c) for t, c in cmds if c.startswith("MI=")]
        self.assertEqual(len(mi), 1)
        self.assertGreaterEqual(mi[0][0], 360000)                          # 60 s enable + 300 s
        self.assertLessEqual(mi[0][0], 360300)

    def test_runtime_enable_on_ot_direct_never_sends_mi500(self):
        script = ["pic 0", "flowper 5000", "flow 50", "enabled 0", "mode 1", "ret unk 0", "unsup28 1", "flame 1", "run 60000", "enabled 1", "run 700000"]
        cmds, _, _, _ = self.sim(script)
        self.assertEqual([c for _, c in cmds if c.startswith("MI=")], [])

    def test_runtime_enable_then_pwm_ignition_floods(self):
        # after the enable the controller enters PWM; the next CH ignition in the ON phase of a no-return boiler floods as at boot
        script = ["pic 1", "flowper 5000", "flow 40", "enabled 0", "mode 1", "ret unk 0", "unsup28 1", "run 60000", "enabled 1", "run 90000",
                  "mode 2", "tick 55", "run 94000", "flame 1", "run 130000"]
        cmds, _, _, _ = self.sim(script)
        self.assertEqual(len(self.pms(cmds, 94100, 114100)), 10)

    # ---- hot boiler in the boiler's own anti-cycle lockout --------------------------------------------------------------
    LOCKOUT = ["pic 1", "flowper 5000", "ret unk 0", "unsup28 1", "flow 52", "slopedown 0.03", "run 100"]

    def test_hot_boiler_in_lockout_holds_the_clamped_flow_hold_and_floods_from_the_flame_not_from_the_on_phase(self):
        # The ON phase starts at 0 with a hot boiler (flow 65, cooling 0.1 C/s) that does not light for 180 s (its own anti-cycle delay).
        script = self.LOCKOUT + ["tick 55", "run 30000", "tick 55", "run 60000", "tick 55", "run 90000", "tick 55", "run 120000", "tick 55",
                                 "run 150000", "tick 55", "run 180000", "q", "flame 1", "run 220000", "q"]
        cmds, _, qs, ticks = self.sim(script)
        self.assertEqual(self.pms(cmds, 0, 180000), [])                    # no PM=25 before the flame: the flood is armed by a flame edge
        pre = [c for t, c in cmds if t < 180000 and c.startswith("CS=")]
        self.assertTrue(pre)
        self.assertTrue(all(float(c[3:]) <= 62.0 for c in pre), pre)        # never above the effective maximum 62
        self.assertTrue(all(float(c[3:]) >= 28.2 for c in pre), pre)        # never below the cold setpoint
        self.assertEqual(qs[0]["phase_on"], "1")                           # the ON phase is still waiting for ignition (Q35)
        fl = self.pms(cmds, 180000)
        self.assertEqual(len(fl), 10)
        self.assertLessEqual(fl[0] - 180000, 1200)                         # the first PM=25 one second after the flame edge
        self.assertEqual(fl[-1] - fl[0], 18000)                            # 10 PM=25 in 20 s: 1, 3 ... 19 s after the flame

    def test_cold_boiler_after_lockout_does_not_flood_before_the_flame(self):
        # the same sequence with a cold boiler: the hold is flow + 18 and the flood still waits for the flame
        script = ["pic 1", "flowper 5000", "ret unk 0", "unsup28 1", "flow 25", "run 100", "tick 55", "run 120000", "q"]
        cmds, _, qs, ticks = self.sim(script)
        self.assertEqual(self.pms(cmds), [])
        self.assertEqual(ticks[0][1], 43.0)                                # 25 + 18


# ---- OT-Direct: the PM= command and the schedule that serves it, on their own code -----------------------------------------------
OTD_STUBS = r"""
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#define PSTR(x) x
#define F(x) x
#define snprintf_P snprintf
#define OTDDebugTf(...) ((void)0)
#define OTDDebugTln(...) ((void)0)
static uint32_t g_ms = 0;
static uint32_t millis() { return g_ms; }
enum class OpenThermMessageType { READ_DATA = 0, WRITE_DATA = 1 };
enum class OpenThermMessageID : int { };
struct OpenTherm { static unsigned long buildRequest(OpenThermMessageType t, OpenThermMessageID id, unsigned int) { return ((unsigned long)t << 16) | (unsigned long)(int)id; } };
struct OTBus { bool bOnline = true; }; struct State { OTBus otBus; }; static State state;
static unsigned long buildStatusRequest() { return 0xFFFF0000UL; }
static bool otIsVentSlave() { return false; }
static bool otCmdQueueEmpty() { return true; }
static bool otCmdEnqueue(unsigned long f) { printf("%lu ENQ %lu\n", (unsigned long)g_ms, f & 0xFFFF); return true; }
static void otDirectBridgeProcessStatus(const char* s) { printf("%lu STATUS %s\n", (unsigned long)g_ms, s); }
static void synthesizeResponse(char*, const char*) {}
static bool otMasterRequestActive = false; static uint32_t otLastAnySendMs = 0; static uint32_t otMinIntervalMs = 100; static uint8_t otScheduleIdx = 0;
static const uint32_t OT_OFFLINE_RETRY_MS = 30000, OT_VENT_FAST_INTERVAL_MS = 10000, OT_SLOW_INTERVAL_MS = 60000;
enum { OT_DIRECT_ORIGIN_GATEWAY = 1 };
static unsigned long otCmdQueue[4]; static uint8_t otCmdTail = 0; static const int OT_CMD_QUEUE_SIZE = 4;
static uint32_t g_busyUntil = 0;
static bool sendMasterRequestAsync(unsigned long frame, int) {
  if (g_ms < g_busyUntil) return false;
  printf("%lu REQ %s %lu\n", (unsigned long)g_ms, ((frame >> 16) & 1) ? "W" : (frame == 0xFFFF0000UL ? "S" : "R"), frame & 0xFFFF);
  otLastAnySendMs = g_ms; g_busyUntil = g_ms + 100;   // MWT 100 ms; the model answers within it (spec: slave answers in 20..400 ms)
  return true;
}
"""

OTD_MAIN = r"""
int main() {
  char line[128];
  while (fgets(line, sizeof line, stdin)) {
    char cmd[16] = {0}; unsigned long a = 0;
    if (sscanf(line, "%15s %lu", cmd, &a) < 1) continue;
    if (!strcmp(cmd, "run")) { while (g_ms < a) { g_ms += 10; scheduleMasterRequest(); } }
    else if (!strcmp(cmd, "pm")) { char v[8]; snprintf(v, sizeof v, "%lu", a); char b[8]; handlePM(v, b); }
    else if (!strcmp(cmd, "disableslow")) { for (uint8_t i = 0; i < OT_SCHEDULE_SIZE; i++) if (otSchedule[i].intervalMs == OT_SLOW_INTERVAL_MS) otSchedule[i].disabled = true; }
  }
  return 0;
}
"""


def build_otd_source():
    o = _read(OTD_INO)
    consts = o[o.index("static constexpr uint32_t OT_STATUS_INTERVAL_MS"):o.index("static constexpr uint32_t OT_WRITE_INTERVAL_MS")]
    consts = consts.split("\n")
    wanted = [l for l in consts if "OT_STATUS_INTERVAL_MS" in l or "OT_TEMP_INTERVAL_MS" in l]
    c2 = o[o.index("static constexpr uint32_t OT_WRITE_INTERVAL_MS"):]
    wanted.append(c2.split("\n")[0])
    start = o.index("struct OTScheduleEntry {")
    end = o.index("static constexpr uint8_t OT_SCHEDULE_SIZE")
    end = o.index("\n", end) + 1
    sched = o[start:end]
    sm = o.index("static void scheduleMasterRequest() {")
    sched_fn = o[sm:o.index("\n}\n", sm) + 3]
    pm = o.index("else if (cmd0 == 'P' && cmd1 == 'M') {")
    pm_end = o.index("\n  }\n", pm) + len("\n  }\n")
    branch = o[pm:pm_end]
    handler = "static bool handlePM(const char* value, char* buf) {\n  const char cmd0 = 'P', cmd1 = 'M'; char rspBuf[16];\n  if (false) {}\n  " + branch + "  return true;\n}\n"
    return (OTD_STUBS + "\n".join(wanted) + "\n" + sched + "\n" + sched_fn + "\n" + handler + OTD_MAIN)


class OtDirectPm(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.dir = tempfile.TemporaryDirectory()
        d = Path(cls.dir.name)
        cpp = d / "t.cpp"
        cpp.write_text(build_otd_source(), encoding="utf-8")
        cls.exe = compile_cpp(cpp, d / "t.exe")

    @classmethod
    def tearDownClass(cls):
        cls.dir.cleanup()

    def sim(self, script):
        if not self.exe:
            self.skipTest("no host C++ compiler")
        out = subprocess.run([str(self.exe)], input="\n".join(script) + "\n", capture_output=True, text=True, check=True).stdout.splitlines()
        return [l.split() for l in out]

    def reads25(self, rows, a=0, b=10**12):
        return [int(r[0]) for r in rows if r[1] == "REQ" and r[2] == "R" and r[3] == "25" and a <= int(r[0]) < b]

    def pm25_latencies(self, slow_tier_enabled):
        """Latency (ms) from each of ten PM=25 commands, 2 s apart (the flood), to the MsgID 25 read on the bus. Steady state, 60 s in,
        PM=25 at an arbitrary phase of the 10 s poll. The slow tier (60 s interval, roughly 80 entries) is what a boiler that answers
        Unknown-DataID to those IDs has switched off by itself."""
        t0 = 61234
        script = ["run 60000"] + ([] if slow_tier_enabled else ["disableslow"])
        for i in range(10):
            script += ["run %d" % (t0 + 2000 * i), "pm 25"]
        script += ["run 90000"]
        rows = self.sim(script)
        sent = self.reads25(rows, t0)
        lat = []
        for a in [t0 + 2000 * i for i in range(10)]:
            nxt = [x for x in sent if x >= a]
            self.assertTrue(nxt, a)
            lat.append(nxt[0] - a)
        return lat

    def test_pm25_goes_out_within_a_communication_interval_when_the_slow_poll_tier_is_idle(self):
        # OpenTherm v4.2 section 4.3.1: the master communicates at least every 1.15 s (MCI max); a PM=25 read goes out within that
        lat = self.pm25_latencies(False)
        self.assertLessEqual(max(lat), 1150, lat)

    def test_pm25_latency_in_the_slow_poll_burst_is_bounded_but_not_one_interval(self):
        # Characterization of the CURRENT OT-Direct scheduler (one request per call, round-robin from otScheduleIdx): when all slow-poll
        # entries come due together (they all start at boot with lastSentMs = 0, so they stay in step), a PM=25 sets lastSentMs = 0 but
        # waits behind the entries already due. The model has no entry disabled; a boiler that answers Unknown-DataID to many of them
        # (the entries disable themselves) has a shorter burst. The bench has to measure the real burst (TASK-1412.21).
        lat = self.pm25_latencies(True)
        self.assertLessEqual(max(lat), 10000, lat)
        self.assertGreater(max(lat), 1150, "the scheduler now serves PM=25 inside one interval in the burst: tighten the assertion above")

    def test_without_pm25_the_msgid25_read_is_only_every_10_s_on_ot_direct(self):
        rows = self.sim(["run 120000"])
        sent = self.reads25(rows, 20000)
        gaps = [b - a for a, b in zip(sent, sent[1:])]
        self.assertTrue(gaps)
        self.assertLessEqual(max(gaps), 10100)
        self.assertGreaterEqual(min(gaps), 10000)

    def test_pm_for_an_unscheduled_id_is_a_one_shot_read(self):
        rows = self.sim(["run 1000", "pm 2"])
        self.assertIn(["1000", "ENQ", "2"], rows)

    def test_pm_out_of_range_is_refused_with_bv(self):
        rows = self.sim(["pm 200"])
        self.assertEqual(rows, [["0", "STATUS", "BV"]])


if __name__ == "__main__":
    unittest.main()
