#!/usr/bin/env python3
"""TASK-1733 / ADR-223: OT-Direct gateway mode answers a TT= or TC= override to the thermostat only, like the PIC.

Decision (Robert; TC= by Sergeant D, entry 286): in gateway mode with a thermostat, TT= / TC= is answer state only.
  - the boiler gets no gateway WRITE 16 and no WRITE 100 for it; every WRITE 16 that reaches the boiler carries the
    thermostat's own data (no 'R' rewritten frame on MsgID 16), and the thermostat's WRITE 16 is acked with its own data;
  - the thermostat's READ of MsgID 9 is answered READ_ACK with the override, its READ of MsgID 100 with the flags
    (TT low byte EXPECT_TT_FLAGS, TC 0x01);
  - release rule (gateway.asm:2948, 2980-3036): counting starts only after the first answered MsgID 9 read; before the
    thermostat accepted (a matching MsgID 16) the 4th mismatching MsgID 16 releases the override, after an acceptance the
    first mismatch does; match window 0.25 C. A thermostat that never reads MsgID 9 never releases it;
  - TT=0 sends no WRITE 100 = 0 and leaves a BS= write override on MsgID 16 alone.
Master mode is out of scope.

The code under test is REAL: handleOTDirectCommand() (prologue, CS/TC/TT/BS branches, epilogue), enqueueWriteCommand(),
clearWriteOverride(), setRemoteOverride(), clearRemoteOverride() and onThermostatMsgID16() are cut from
src/OTGW-firmware/OTDirect.ino; the gateway loop (applyOverrides, replyToThermostat, otdRemoteOverrideReply, the
alternative slots, loopOTDirect) comes from the slices of tests/test_otd_gateway_alt_slots.py. Only the hardware, the
clock, the bridge log and the command-confirmation hook (synthesizeResponse) are doubles.

Non-vacuous: in gateway mode with a thermostat the command ring drains only in alternative slots (OTDirect.ino
loopOTDirect). Every case first warms up 70 s with a boiler that answers MsgID 24 Unknown-DataID, so MsgID 24 becomes
an alternative slot; test_00 checks that, and that a probe WRITE 14 pushed through the same ring reaches the boiler.
Without that a 'no gateway WRITE 16' check would also be green on code that does send them.

Old-vs-fix: OTGW_OTD_INO points at another copy of OTDirect.ino and OTGW_OTD_TYPES at the OTDirecttypes.h of the same
revision (struct OTRemoteOverrideState differs between the two). Recipe for the red run on the code before ADR-223:
  git show <old-rev>:src/OTGW-firmware/OTDirect.ino      > $TMP/OTDirect.ino
  git show <old-rev>:src/OTGW-firmware/OTDirecttypes.h   > $TMP/OTDirecttypes.h
  OTGW_OTD_INO=$TMP/OTDirect.ino OTGW_OTD_TYPES=$TMP/OTDirecttypes.h python tests/test_otd_tt_pic_parity.py
The two variables go together: the .ino of one revision with the types header of another does not compile.
Run: python tests/test_otd_tt_pic_parity.py
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
import test_otd_gateway_alt_slots as alt  # noqa: E402
from test_sat_pwm_override import compile_cpp  # noqa: E402

OTD_INO = alt.OTD_INO
if os.environ.get("OTGW_OTD_TYPES"):
    alt.TYPES_H = Path(os.environ["OTGW_OTD_TYPES"])   # alt.lib_and_types() reads the types header from here

# ADR-223 numbers. EXPECT_TT_FLAGS is a named constant so the TT flags value can change on its own
# (gateway.asm:4061 uses b'11' for TT; the firmware answers 0x0002 today).
OVR_F88 = 0x1580          # TT=21.5 / TC=21.5
THERMO_F88 = 0x1400       # the thermostat's own room setpoint, 20.0
EXPECT_TT_FLAGS = 0x0002
EXPECT_TC_FLAGS = 0x0001
MATCH_F88 = 0x0040        # 0.25 C, OT_OVERRIDE_HONOR_DELTA_F88 (ADR-223 sub-point 3)
RELEASE_N = 4             # before acceptance the 4th mismatch releases (gateway.asm:3025-3036)

STUBS = r"""
#include <cctype>
static int g_other = 0;
static struct DiagRec { bool active = false; void recOtdCommand(const char*, size_t) {} } diagRec;
static bool satBlocksExternalCommand(const char*, int) { return false; }
static void satNoteExternalRefused(const char*, int) {}
static std::vector<std::string> g_ng, g_synthLog;
static void otDirectBridgeProcessStatus(const char* s) { g_ng.push_back(s); }
static char* dtostrf(float v, int, int, char* b) { snprintf(b, 16, "%.2f", v); return b; }
static void synthesizeResponse(const char* c, const char* v) { g_synthLog.push_back(std::string(c, 2) + ": " + v); }
// prototypes the Arduino .ino preprocessor generates; the real definitions come from the slices
static bool enqueueWriteCommand(uint8_t msgId, uint16_t dataValue, const char* label);
static void onThermostatMsgID16(uint8_t msgType, uint16_t f88);
static void clearWriteOverride(uint8_t msgId);
static void setRemoteOverride(OTRemoteOverrideMode mode, float celsius);
static void clearRemoteOverride();
"""

MAIN2 = r"""
static uint8_t idOf(unsigned long f) { return (uint8_t)((f >> 16) & 0xFF); }
static uint8_t typeOf(unsigned long f) { return (uint8_t)((f >> 28) & 7); }
static const char* mname(OTRemoteOverrideMode m) {
  return m == OT_OVERRIDE_NONE ? "NONE" : m == OT_OVERRIDE_TEMPORARY ? "TEMPORARY" : "CONSTANT";
}

// ---- thermostat stream: 14 frames, one per second --------------------------------
// idx 7 = READ 9, idx 9 = WRITE 16, idx 13 = READ 100 (reads only when g_readsMode says so, else READ 27)
static int g_readsMode = 0;   // 0 never, 1 always, 2 from the 7th cycle after the command
static int g_w16Mode = 0;     // 0 always 0x1400, 1 echoes the override, 2 echoes twice then 0x1400
static bool g_ttCalled = false;
static int g_cycleFrames = 0;
static size_t g_pos = 0;
static size_t g_slaveScan = 0;
static bool g_ans9 = false;
static int g_pend = -1;
static int g_reads9 = 0, g_reads100 = 0;
static std::vector<uint16_t> g_thermoW16;
struct FrameRec { int j; uint16_t data; bool ans9; bool active; };
static std::vector<FrameRec> g_fr;

static bool readsOn() {
  if (g_readsMode == 1) return true;
  if (g_readsMode == 2) return g_ttCalled && g_cycleFrames >= 6 * 14;
  return false;
}

static void thermoTick() {
  if (g_pend >= 0) { g_fr[g_pend].active = otRemoteOverride.mode != OT_OVERRIDE_NONE; g_pend = -1; }
  size_t idx = g_pos++ % 14;
  unsigned long f;
  switch (idx) {
    case 1:  f = frameOf(1, 1, 0x3200); break;
    case 3:  f = frameOf(0, 25, 0); break;
    case 5:  f = frameOf(0, 17, 0); break;
    case 7:  f = readsOn() ? frameOf(0, 9, 0) : frameOf(0, 27, 0); break;
    case 11: f = frameOf(1, 24, 0x1499); break;
    case 13: f = readsOn() ? frameOf(0, 100, 0) : frameOf(0, 27, 0); break;
    case 9: {
      uint16_t d = 0x1400;
      if (g_ttCalled) {
        size_t j = g_thermoW16.size();
        if (g_w16Mode == 1) d = 0x1580;
        else if (g_w16Mode == 2 && j < 2) d = 0x1580;
        g_thermoW16.push_back(d);
        g_fr.push_back({ (int)g_thermoW16.size(), d, g_ans9, false });
        g_pend = (int)g_fr.size() - 1;
      }
      f = frameOf(1, 16, d);
      break;
    }
    default: f = frameOf(0, 0, 0x0200); break;
  }
  if (g_ttCalled) {
    g_cycleFrames++;
    if (idOf(f) == 9) g_reads9++;
    if (idOf(f) == 100) g_reads100++;
  }
  handleSlaveRequest(f, OpenThermResponseStatus::SUCCESS);
  state.otd.bThermostatConnected = true;
}

static void step(uint32_t ms) {
  for (uint32_t end = g_now + ms; g_now < end; g_now += 2) {
    if (g_now % 1000 == 0) thermoTick();
    boilerTick();
    loopOTDirect();
    while (g_slaveScan < otSlave.sent.size()) {
      unsigned long a = otSlave.sent[g_slaveScan++];
      if (g_ttCalled && idOf(a) == 9) g_ans9 = true;
    }
  }
  if (g_pend >= 0) { g_fr[g_pend].active = otRemoteOverride.mode != OT_OVERRIDE_NONE; g_pend = -1; }
}

static int countAltSlotsOf(int id) {
  int n = 0;
  for (size_t i = 0; i + 1 < g_log.size(); i++)
    if (g_log[i].prefix == 'T' && idOf(g_log[i].frame) == id && g_log[i + 1].prefix == 'A' && typeOf(g_log[i + 1].frame) == 7) n++;
  return n;
}
static int countAltSlots() { return countAltSlotsOf(24); }

static size_t g_fromLog, g_fromSent, g_fromSlave;
static void mark() { g_fromLog = g_log.size(); g_fromSent = otMaster.sent.size(); g_fromSlave = otSlave.sent.size(); }

// ---- report ------------------------------------------------------------------------
static void report(const char* tag, size_t fromLog, size_t fromSent, size_t fromSlave) {
  printf("WIN %s\n", tag);
  for (size_t i = fromSent; i < otMaster.sent.size(); i++) {
    unsigned long q = otMaster.sent[i];
    if (typeOf(q) == 1 && idOf(q) == 16) printf("BW16 %04X\n", (unsigned)(q & 0xFFFF));
    if (typeOf(q) == 1 && idOf(q) == 100) printf("BW100 %04X\n", (unsigned)(q & 0xFFFF));
    if (typeOf(q) == 1 && idOf(q) == 14) printf("BW14 %04X\n", (unsigned)(q & 0xFFFF));
  }
  int r16 = 0;
  for (size_t i = fromLog; i < g_log.size(); i++) if (g_log[i].prefix == 'R' && idOf(g_log[i].frame) == 16) r16++;
  printf("R16 %d\n", r16);
  for (size_t i = fromSlave; i < otSlave.sent.size(); i++) {
    unsigned long a = otSlave.sent[i];
    uint8_t id = idOf(a);
    int par = OpenTherm::parity(a) ? 1 : 0;
    if (id == 16) printf("RP16 %u %04X %d\n", typeOf(a), (unsigned)(a & 0xFFFF), par);
    if (id == 9) printf("RP9 %u %04X %d\n", typeOf(a), (unsigned)(a & 0xFFFF), par);
    if (id == 100) printf("RP100 %u %04X %d\n", typeOf(a), (unsigned)(a & 0xFFFF), par);
  }
  for (size_t i = 0; i < g_thermoW16.size(); i++) printf("TW16 %04X\n", g_thermoW16[i]);
  for (auto& fr : g_fr) printf("FR %d %04X %d %d\n", fr.j, fr.data, fr.ans9 ? 1 : 0, fr.active ? 1 : 0);
  printf("READS %d %d\n", g_reads9, g_reads100);
  for (uint8_t i = 0; i < OT_SCHEDULE_SIZE; i++)
    if (otSchedule[i].isWrite && otSchedule[i].msgId == 16) printf("VS16 %d\n", otSchedule[i].valueSet ? 1 : 0);
  printf("ALT16 %d\n", countAltSlotsOf(16));
  printf("END %s\n", mname(otRemoteOverride.mode));
}

static void setupCase(int reads, int w16, bool no16 = false) {
  resetAll(); g_no24 = true; g_no16 = no16;
  g_readsMode = reads; g_w16Mode = w16;
  step(98000);      // 98 frames = 7 cycles: 24 becomes an alternative slot, the next frame is idx 0
  printf("PRE altslots=%d frameidx=%d\n", countAltSlots(), (int)(g_pos % 14));
}

static bool command(const char* cmd) {
  bool ret = handleOTDirectCommand(cmd, (int)strlen(cmd), false);
  printf("CMD %s ret=%d ng=%d mode=%s f88=%04X smode=%s sf88=%04X\n", cmd, ret ? 1 : 0, (int)g_ng.size(),
         mname(otRemoteOverride.mode), (unsigned)otRemoteOverride.f88Value,
         mname(state.otd.eOverrideMode), (unsigned)state.otd.iOverrideF88);
  return ret;
}

static void dumpQueue(const char* tag) {
  for (uint8_t i = otCmdTail; i != otCmdHead; i = (i + 1) % OT_CMD_QUEUE_SIZE)
    printf("Q %s %u %04X\n", tag, (unsigned)typeOf(otCmdQueue[i]) * 1000 + idOf(otCmdQueue[i]), (unsigned)(otCmdQueue[i] & 0xFFFF));
}

static void startOverride(const char* cmd) {
  mark();
  command(cmd);
  g_ttCalled = true; g_slaveScan = otSlave.sent.size();
}

int main(int argc, char** argv) {
  std::string c = argc > 1 ? argv[1] : "";
  const char* cmd = (c.size() > 2 && c.compare(c.size() - 2, 2, "TC") == 0) ? "TC=21.5" : "TT=21.5";
  if (c == "P") {
    setupCase(0, 0);
    mark();
    otCmdEnqueue(OpenTherm::buildRequest(OpenThermMessageType::WRITE_DATA, (OpenThermMessageID)14, 0x3200));
    step(60000);
    report("P", g_fromLog, g_fromSent, g_fromSlave);
  } else if (c.rfind("A_", 0) == 0) { setupCase(0, 0); startOverride(cmd); step(120000); report("A", g_fromLog, g_fromSent, g_fromSlave); }
  else if (c.rfind("B_", 0) == 0) { setupCase(1, 0); startOverride(cmd); step(120000); report("B", g_fromLog, g_fromSent, g_fromSlave); }
  else if (c.rfind("E_", 0) == 0) { setupCase(1, 1); startOverride(cmd); step(120000); report("E", g_fromLog, g_fromSent, g_fromSlave); }
  else if (c.rfind("D_", 0) == 0) { setupCase(1, 2); startOverride(cmd); step(120000); report("D", g_fromLog, g_fromSent, g_fromSlave); }
  else if (c.rfind("G_", 0) == 0) { setupCase(2, 0); startOverride(cmd); step(180000); report("G", g_fromLog, g_fromSent, g_fromSlave); }
  else if (c == "F") {
    setupCase(0, 0);
    command("BS=19.0");                 // a BS= fake room setpoint: write override on MsgID 16
    startOverride("TT=21.5");
    step(30000);
    mark();                             // the window after TT=0
    command("TT=0");
    step(60000);
    report("F", g_fromLog, g_fromSent, g_fromSlave);
  }
  else if (c.rfind("H_", 0) == 0) {          // MsgID 16 is an alternative slot: the boiler answers WRITE 16 Unknown-DataID
    setupCase(1, 0, true); startOverride(cmd); step(120000); report("H", g_fromLog, g_fromSent, g_fromSlave);
  }
  else if (c == "N") {                        // no override: the boiler rejects MsgID 9, the thermostat keeps reading it
    setupCase(1, 0); mark(); step(120000); report("N", g_fromLog, g_fromSent, g_fromSlave);
  }
  else if (c == "BS" || c == "MQ") {          // BS=0 clears a BS= rewrite and an MQTT room_setpoint write on MsgID 16
    setupCase(0, 0);
    if (c == "BS") command("BS=19.0");
    else enqueueWriteCommand(16, 0x1300, "MQTT-room_setpoint");
    step(30000);
    command("BS=0");
    step(3000);
    mark();                                   // the window after BS=0
    step(60000);
    report(c.c_str(), g_fromLog, g_fromSent, g_fromSlave);
  }
  else if (c == "M") {                        // master mode: nobody reads MsgID 9, TT= writes TrSet and the flags
    resetAll(); otCurrentMode = OTD_MODE_MASTER;
    command("TT=21.5"); dumpQueue("TT");
    otCmdTail = otCmdHead;
    command("TT=0"); dumpQueue("TT0");
    otCmdTail = otCmdHead;
    command("TC=21.5"); dumpQueue("TC");
  }
  else { printf("unknown case %s\n", c.c_str()); return 2; }
  return 0;
}
"""


def _rep(s, old, new):
    assert old in s, f"harness anchor not found in alt_slots PRE: {old!r}"
    return s.replace(old, new, 1)


def build_source(ino_path):
    src = Path(ino_path).read_text(encoding="utf-8").replace("\r\n", "\n")
    enums, statics, types = alt.lib_and_types()
    pre = alt.PRE
    # the real onThermostatMsgID16 / clearWriteOverride replace the alt_slots no-op stubs
    pre = _rep(pre, "static void onThermostatMsgID16(uint8_t, uint16_t) {}\n", "")
    pre = _rep(pre, "static void clearWriteOverride(uint8_t) {}\n", "")
    pre = _rep(pre, "struct { bool bThermostatConnected; } otd;",
               "struct { bool bThermostatConnected; OTRemoteOverrideMode eOverrideMode; uint16_t iOverrideF88; } otd;")
    pre = pre.replace("@LIB_ENUMS@", enums).replace("@LIB_STATICS@", statics).replace("@TYPES@", types)

    # handler: prologue + CS/TC/TT/BS branches (cut before the C2 branch, anchored on code) + stub else + epilogue
    h0 = src.index("bool handleOTDirectCommand(const char* buf, int len, bool satOwn) {")
    c2 = re.compile(r"^  else if \(cmd0 == 'C' && cmd1 == '2'\)", re.M).search(src, h0)
    assert c2, "C2 branch anchor not found"
    assert "return !otdWriteRefused;" in src[c2.start():], "handler epilogue not found"
    assert "cmd0 == 'T' && cmd1 == 'T'" in src[h0:c2.start()] and "cmd0 == 'B' && cmd1 == 'S'" in src[h0:c2.start()]
    handler = src[h0:c2.start()] + "  else { g_other++; }\n  return !otdWriteRefused;\n}\n"

    extra = "\n".join([
        alt.construct(src, r"^// TASK-1593: set when"),
        alt.construct(src, r"^static bool enqueueWriteCommand\([^;{]*\)\s*\{"),
        alt.construct(src, r"^static void clearWriteOverride\([^;{]*\)\s*\{"),
        alt.construct(src, r"^static void setRemoteOverride\([^;{]*\)\s*\{"),
        alt.construct(src, r"^static void clearRemoteOverride\([^;{]*\)\s*\{"),
        alt.construct(src, r"^static void onThermostatMsgID16\([^;{]*\)\s*\{"),
        # TASK-1842: the RT= branch (before C2, so inside the cut) calls the PIC GetPercentArg parser
        (alt.construct(src, r"^static const char\* otdPicPercentF88\([^;{]*\)\s*\{")
         if "// --- otdPicPercentF88 begin" in src else ""),
        handler,
    ])
    main_prefix = alt.MAIN[:alt.MAIN.index("struct Run {")]   # boiler model, frameOf, boilerTick, resetAll
    main_prefix = _rep(main_prefix, "static bool g_no24 = false;", "static bool g_no24 = false;\nstatic bool g_no16 = false;   // the boiler answers WRITE 16 Unknown-DataID")
    main_prefix = _rep(main_prefix, "if (g_no24 && id == 24) return false;", "if (g_no24 && id == 24) return false;\n  if (g_no16 && id == 16) return false;")
    return pre + STUBS + alt.slices(src) + "\n" + extra + "\n" + main_prefix + MAIN2


CASES_TT = ["A_TT", "B_TT", "E_TT", "D_TT", "G_TT"]
CASES_TC = ["A_TC", "B_TC", "E_TC", "D_TC", "G_TC"]
CASES_H = ["H_TT", "H_TC"]
ALL_CASES = ["P"] + CASES_TT + CASES_TC + CASES_H + ["F", "N", "BS", "MQ", "M"]
RES = {}
ERR = None
TMP = None


def setUpModule():
    global ERR, TMP
    TMP = tempfile.mkdtemp(prefix="otd_tt_parity_")
    cpp = Path(TMP) / "t.cpp"
    cpp.write_text(build_source(OTD_INO), encoding="utf-8")
    exe = compile_cpp(cpp, Path(TMP) / ("t.exe" if os.name == "nt" else "t"))
    if exe is None:
        ERR = "SKIP no host compiler"
        return
    for name in ALL_CASES:
        p = subprocess.run([str(exe), name], capture_output=True, text=True, check=True)
        RES[name] = parse(p.stdout)
        print(f"--- {name} ---\n" + "".join(l + "\n" for l in p.stdout.splitlines()
                                            if not l.startswith(("FR ", "TW16", "RP16", "RP9", "RP100", "BW16"))), end="")
        r = RES[name]
        if name == "M":
            print("    queue=" + str(r["q"]))
            continue
        print(f"    bw16={len(r['bw16'])} thermo_w16={len(r['tw16'])} bw100={len(r['bw100'])} r16={r['r16']} "
              f"rp9={len(r['rp9'])} rp100={len(r['rp100'])} reads={r['reads']} end={r['end']}")


def tearDownModule():
    if TMP:
        shutil.rmtree(TMP, ignore_errors=True)


def parse(out):
    r = {"bw16": [], "bw100": [], "bw14": [], "r16": 0, "rp16": [], "rp9": [], "rp100": [], "tw16": [], "fr": [],
         "reads": (0, 0), "end": None, "pre": None, "cmd": {}, "vs16": [], "alt16": 0, "q": {}}
    for line in out.splitlines():
        t = line.split()
        if not t:
            continue
        k = t[0]
        if k == "BW16": r["bw16"].append(int(t[1], 16))
        elif k == "BW100": r["bw100"].append(int(t[1], 16))
        elif k == "BW14": r["bw14"].append(int(t[1], 16))
        elif k == "R16": r["r16"] = int(t[1])
        elif k in ("RP16", "RP9", "RP100"): r[k.lower()].append((int(t[1]), int(t[2], 16), int(t[3])))
        elif k == "TW16": r["tw16"].append(int(t[1], 16))
        elif k == "FR": r["fr"].append({"j": int(t[1]), "data": int(t[2], 16), "ans9": bool(int(t[3])), "active": bool(int(t[4]))})
        elif k == "READS": r["reads"] = (int(t[1]), int(t[2]))
        elif k == "END": r["end"] = t[1]
        elif k == "VS16": r["vs16"].append(int(t[1]))
        elif k == "ALT16": r["alt16"] = int(t[1])
        elif k == "Q": r["q"].setdefault(t[1], []).append((int(t[2]), int(t[3], 16)))
        elif k == "PRE": r["pre"] = {a.split("=")[0]: int(a.split("=")[1]) for a in t[1:]}
        elif k == "CMD":
            r["cmd"][t[1]] = {a.split("=")[0]: a.split("=")[1] for a in t[2:]}
    return r


def hexl(lst, n=6):
    return "[" + ", ".join(f"{x:04X}" for x in lst[:n]) + (", ..." if len(lst) > n else "") + f"] (n={len(lst)})"


class Base(unittest.TestCase):
    def setUp(self):
        if ERR:
            self.skipTest(ERR)


class Precondition(Base):
    def test_00_alternative_slots_exist_and_the_ring_drains(self):
        for name in [n for n in ALL_CASES if n != "M"]:
            self.assertGreaterEqual(RES[name]["pre"]["altslots"], 3,
                                    f"{name}: MsgID 24 did not become an alternative slot; the ring would never drain")
        p = RES["P"]
        self.assertEqual(p["bw14"], [0x3200], "a probe WRITE 14 pushed into the command ring must reach the boiler in a slot "
                                              "(otherwise 'no gateway WRITE 16' would be green on any code)")


class ParityCases:
    KIND = None          # "TT" or "TC"

    def r(self, letter):
        return RES[f"{letter}_{self.KIND}"]

    def flags(self):
        return EXPECT_TT_FLAGS if self.KIND == "TT" else EXPECT_TC_FLAGS

    def test_state_after_command(self):
        c = self.r("A")["cmd"][f"{self.KIND}=21.5"]
        self.assertEqual(c["ret"], "1")
        self.assertEqual(c["mode"], "TEMPORARY" if self.KIND == "TT" else "CONSTANT")
        self.assertEqual(c["f88"], f"{OVR_F88:04X}")
        self.assertEqual((c["smode"], c["sf88"]), (c["mode"], c["f88"]), "state.otd must mirror the override")
        self.assertEqual(c["ng"], "0")

    def test_A1_boiler_write16_is_only_the_thermostats_own(self):
        r = self.r("A")
        self.assertGreater(len(r["tw16"]), 5)
        self.assertEqual(r["bw16"], r["tw16"][:len(r["bw16"])],
                         f"WRITE 16 at the boiler differs from the thermostat's own: boiler {hexl(r['bw16'])} vs thermostat {hexl(r['tw16'])}")
        self.assertLessEqual(len(r["tw16"]) - len(r["bw16"]), 1, "thermostat WRITE 16 frames did not all reach the boiler")

    def test_A2_no_write100_to_the_boiler(self):
        self.assertEqual(self.r("A")["bw100"], [], f"the boiler received WRITE 100: {hexl(self.r('A')['bw100'])}")

    def test_A3_no_rewritten_frame_on_msgid16(self):
        self.assertEqual(self.r("A")["r16"], 0, "the thermostat's WRITE 16 was rewritten (R frame on MsgID 16)")

    def test_A4_thermostat_write16_acked_with_own_data(self):
        r = self.r("A")
        self.assertGreater(len(r["rp16"]), 5)
        for k, (typ, data, par) in enumerate(r["rp16"][:len(r["tw16"])]):
            self.assertEqual((typ, data, par), (5, r["tw16"][k], 0), f"ack #{k} of the thermostat's WRITE 16 is not WRITE_ACK with its own data")

    def test_A5_without_msgid9_reads_the_override_never_releases(self):
        r = self.r("A")
        self.assertEqual(r["reads"], (0, 0))
        self.assertTrue(all(f["active"] for f in r["fr"]) and r["fr"], "released although the thermostat never read MsgID 9")
        self.assertNotEqual(r["end"], "NONE")

    def test_B1_msgid9_read_answered_with_the_override(self):
        r = self.r("E")
        self.assertGreater(r["reads"][0], 5)
        self.assertGreaterEqual(len(r["rp9"]), r["reads"][0] - 1)
        for k, row in enumerate(r["rp9"]):
            self.assertEqual(row, (4, OVR_F88, 0), f"MsgID 9 reply #{k} is not READ_ACK {OVR_F88:04X} with even parity")

    def test_B2_msgid100_read_answered_with_the_flags(self):
        r = self.r("E")
        self.assertGreater(r["reads"][1], 5)
        self.assertGreaterEqual(len(r["rp100"]), r["reads"][1] - 1)
        for k, (typ, data, par) in enumerate(r["rp100"]):
            self.assertEqual((typ, data & 0xFF, par), (4, self.flags(), 0), f"MsgID 100 reply #{k}: {typ}/{data:04X}")

    def test_E_echo_keeps_the_override_and_adds_no_boiler_traffic(self):
        r = self.r("E")
        self.assertTrue(r["fr"] and all(f["active"] for f in r["fr"]), "an echoing thermostat released the override")
        self.assertEqual(r["bw16"], r["tw16"][:len(r["bw16"])], f"boiler {hexl(r['bw16'])} vs thermostat {hexl(r['tw16'])}")
        self.assertLessEqual(len(r["tw16"]) - len(r["bw16"]), 1)
        self.assertEqual((r["bw100"], r["r16"]), ([], 0))

    def test_C1_release_at_the_4th_mismatch_after_the_first_answered_msgid9_read(self):
        fr = self.r("B")["fr"]
        f0 = next((i for i, f in enumerate(fr) if f["ans9"]), None)
        self.assertIsNotNone(f0, "no MsgID 9 read was answered")
        self.assertTrue(all(f["active"] for f in fr[:f0 + RELEASE_N - 1]), f"released before the {RELEASE_N}th mismatch: {[(f['j'], f['active']) for f in fr[:f0 + 4]]}")
        self.assertFalse(fr[f0 + RELEASE_N - 1]["active"], f"still active after {RELEASE_N} mismatching MsgID 16 frames (first answered read at frame {fr[f0]['j']})")
        self.assertEqual(self.r("B")["end"], "NONE")

    def test_C2_counting_starts_only_after_the_first_msgid9_read(self):
        fr = self.r("G")["fr"]
        f0 = next((i for i, f in enumerate(fr) if f["ans9"]), None)
        self.assertIsNotNone(f0, "no MsgID 9 read was answered")
        self.assertGreaterEqual(f0, RELEASE_N + 1, "precondition: enough mismatching frames before the first read to expose counting from the command")
        self.assertTrue(all(f["active"] for f in fr[:f0 + RELEASE_N - 1]),
                        f"released too early: mismatches before the first MsgID 9 read must not count ({[(f['j'], f['active']) for f in fr[max(0, f0 - 1):f0 + 4]]})")
        self.assertFalse(fr[f0 + RELEASE_N - 1]["active"], "not released after 4 mismatches following the first answered read")

    def test_D1_after_acceptance_the_first_mismatch_releases(self):
        r = self.r("D")
        fr = r["fr"]
        f0 = next((i for i, f in enumerate(fr) if f["ans9"]), None)
        self.assertIsNotNone(f0)
        self.assertEqual([f["data"] for f in fr[f0:f0 + 3]], [OVR_F88, OVR_F88, THERMO_F88], "harness: thermostat accepts twice, then goes back")
        self.assertTrue(fr[f0]["active"] and fr[f0 + 1]["active"], "released while the thermostat echoed the override")
        self.assertFalse(fr[f0 + 2]["active"], "still active after the first mismatch following an acceptance")
        self.assertEqual(r["r16"], 0)
        self.assertEqual(r["bw16"], r["tw16"][:len(r["bw16"])], f"boiler {hexl(r['bw16'])} vs thermostat {hexl(r['tw16'])}")
        self.assertEqual(r["bw100"], [])


class ParityTT(ParityCases, Base):
    KIND = "TT"


class ParityTC(ParityCases, Base):
    KIND = "TC"


class ClearCases(Base):
    def test_F1_tt0_sends_no_write100_to_the_boiler(self):
        r = RES["F"]
        self.assertEqual(r["cmd"]["TT=0"]["ret"], "1")
        self.assertEqual(r["bw100"], [], f"TT=0 put WRITE 100 on the boiler bus: {hexl(r['bw100'])}")

    def test_F2_tt0_leaves_the_bs_write_override_on_msgid16(self):
        r = RES["F"]
        self.assertEqual(r["cmd"]["TT=0"]["mode"], "NONE")
        self.assertGreaterEqual(len(r["bw16"]), 3)
        self.assertEqual(set(r["bw16"]), {0x1300}, f"after TT=0 the BS=19.0 override (0x1300) must still rewrite the thermostat's WRITE 16; boiler got {hexl(r['bw16'])}")


class AltSlotCases:
    """MsgID 16 is an alternative slot (the boiler answers WRITE 16 Unknown-DataID): the thermostat's frame never goes
    through applyOverrides(), yet the release rule still counts it (gateway.asm TreatMessage runs before the swap)."""
    KIND = None

    def test_H1_msgid16_is_an_alternative_slot(self):
        r = RES[f"H_{self.KIND}"]
        self.assertGreaterEqual(r["alt16"], 3, "precondition: the thermostat's WRITE 16 must be answered Unknown-DataID in a slot")
        self.assertEqual(r["bw16"], [], f"precondition: no WRITE 16 reaches the boiler as a slot request: {hexl(r['bw16'])}")

    def test_H2_release_after_4_mismatches_once_msgid9_was_answered(self):
        fr = RES[f"H_{self.KIND}"]["fr"]
        f0 = next((i for i, f in enumerate(fr) if f["ans9"]), None)
        self.assertIsNotNone(f0, "no MsgID 9 read was answered")
        self.assertTrue(all(f["active"] for f in fr[:f0 + RELEASE_N - 1]),
                        f"released before the {RELEASE_N}th mismatch: {[(f['j'], f['active']) for f in fr[:f0 + 4]]}")
        self.assertFalse(fr[f0 + RELEASE_N - 1]["active"],
                         f"MsgID 16 was an alternative slot and the override stayed active after {RELEASE_N} mismatching frames")
        self.assertEqual(RES[f"H_{self.KIND}"]["end"], "NONE")

    def test_H3_each_frame_counted_once(self):
        # a second count per frame would release at the 2nd mismatch instead of the 4th
        fr = RES[f"H_{self.KIND}"]["fr"]
        f0 = next(i for i, f in enumerate(fr) if f["ans9"])
        self.assertTrue(fr[f0 + 1]["active"] and fr[f0 + 2]["active"])


class AltSlotTT(AltSlotCases, Base):
    KIND = "TT"


class AltSlotTC(AltSlotCases, Base):
    KIND = "TC"


class Msgid9Cases(Base):
    def test_N1_rejected_msgid9_without_override_is_read_ack_zero(self):
        r = RES["N"]
        self.assertGreater(len(r["rp9"]), 5, "harness: the thermostat reads MsgID 9")
        for k, row in enumerate(r["rp9"]):
            self.assertEqual(row, (4, 0, 0), f"MsgID 9 reply #{k} without an override is type {row[0]} data {row[1]:04X}, not READ_ACK 0 (gateway.asm MessageID9)")

    def test_N2_msgid9_is_no_alternative_slot(self):
        # the alternative slots of the warm-up (MsgID 24) exist, MsgID 9 is not among them
        r = RES["N"]
        self.assertGreaterEqual(r["pre"]["altslots"], 3)
        self.assertEqual(r["bw14"], [])


class BsClearCases(Base):
    def test_B1_bs0_clears_a_bs_rewrite(self):
        r = RES["BS"]
        self.assertGreaterEqual(len(r["bw16"]), 3)
        self.assertEqual(set(r["bw16"]), {0x1400}, f"after BS=0 the thermostat's own WRITE 16 must reach the boiler: {hexl(r['bw16'])}")
        self.assertEqual(r["r16"], 0)

    def test_B2_bs0_clears_an_mqtt_room_setpoint_write(self):
        r = RES["MQ"]
        self.assertGreaterEqual(len(r["bw16"]), 3)
        self.assertEqual(set(r["bw16"]), {0x1400}, f"after BS=0 an MQTT room_setpoint write must be gone: {hexl(r['bw16'])}")
        self.assertEqual(r["r16"], 0)
        self.assertTrue(r["vs16"] and not any(r["vs16"]), "the MsgID 16 schedule write value must be cleared, not refreshed")


class MasterModeCases(Base):
    def test_M1_master_mode_tt_writes_trset_and_flags(self):
        q = RES["M"]["q"]
        self.assertEqual(sorted(q["TT"]), sorted([(1016, OVR_F88), (1100, EXPECT_TT_FLAGS)]), f"TT=21.5 in master mode queued {q['TT']}")

    def test_M2_master_mode_tt0_writes_flags_zero(self):
        self.assertEqual(RES["M"]["q"]["TT0"], [(1100, 0)])

    def test_M3_master_mode_tc_writes_trset_and_flags(self):
        q = RES["M"]["q"]
        self.assertEqual(sorted(q["TC"]), sorted([(1016, OVR_F88), (1100, EXPECT_TC_FLAGS)]), f"TC=21.5 in master mode queued {q['TC']}")


if __name__ == "__main__":
    unittest.main(verbosity=2)
