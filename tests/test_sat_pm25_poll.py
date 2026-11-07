#!/usr/bin/env python3
"""TASK-1653: SAT on a PIC board asks for MsgID 25 itself (PM=25) when the thermostat never reads it.

Nothing is re-implemented. The host program is cut out of the production sources:
  - SATboilerFeed.h and SATpm25Poll.h as they are,
  - the PIC command queue of OTGW-Core.ino (the same cut as tests/test_pic_pm_chain.py: addCommandToQueue(),
    handleCommandQueue() with the priority-slot gate, checkCommandResponse(), the PM chain, pmChainBusy(), picPrioSlotBusy()),
  - from processOT(): the frame parse, the one-frame-delay pairing block (which holds the new satPm25NoteFrame() hook), the
    pmChainOnBoilerResponse() line and the flow/return feed hook, in their source order,
  - satPm25PollStep() and its statics from SATcontrol.ino.
The harness models the bus: a thermostat that sends one request per second from a list of DataIDs, a boiler that rejects some
of them (Unknown-DataID), and a PIC that echoes every command and serves a pending PM= in the next request for a DataID the
boiler rejects (T X, R 25, B 25, A X), as gateway.asm CreateRequest/Alternative/HandleResponse do.

Not reached here: the rest of processOT() (decoding, MQTT, logging), the call site in satControlLoopRun() (checked at source
level), the real PIC (6.8 on the bench) and the timing of a real OpenTherm bus.

Old-vs-fix: OTGW_CORE_INO / OTGW_SAT_INO / OTGW_PM25_H point at other copies (e.g. `git show HEAD:...`; a missing PM25 header
means the code before TASK-1653, which has no step: the harness then sends nothing and the cases go red, not the build).

Run: python tests/test_sat_pm25_poll.py
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
import test_pic_pm_chain as pmc  # noqa: E402

FW = REPO / "src" / "OTGW-firmware"
CORE_INO = Path(os.environ.get("OTGW_CORE_INO", FW / "OTGW-Core.ino"))
SAT_INO = Path(os.environ.get("OTGW_SAT_INO", FW / "SATcontrol.ino"))
PM25_H = Path(os.environ.get("OTGW_PM25_H", FW / "SATpm25Poll.h"))
FEED_H = FW / "SATboilerFeed.h"


def _read(p):
    return Path(p).read_text(encoding="utf-8").replace("\r\n", "\n")


def _cut(src, start, end_marker, include_end=True):
    a = src.index(start)
    b = src.index(end_marker, a)
    return src[a:b + (len(end_marker) if include_end else 0)]


HARNESS = r"""
// ---- processOT() pieces ------------------------------------------------------------------------------------------------------
enum { OTGW_BOILER = 1, OTGW_THERMOSTAT, OTGW_REQUEST_BOILER, OTGW_ANSWER_THERMOSTAT, OTGW_PARITY_ERROR };
enum { OT_READ_DATA = 0, OT_WRITE_DATA = 1, OT_READ_ACK = 4, OT_WRITE_ACK = 5, OT_DATA_INVALID = 6, OT_UNKNOWN_DATA_ID = 7 };
enum { OT_Tboiler = 25, OT_Tret = 28 };
struct OTframe {
  char buf[16]; int len; uint32_t value; uint8_t type, masterslave, id, valueHB, valueLB; uint32_t time;
  bool skipthis, bGatewaySubstituted, bAnswerOverride, bLocalAnswer, bReplayed; int rsptype;
};
static OTframe OTdata, delayedOTdata, tmpOTdata;
#define siscanf sscanf
#define OTDebugln(...)
static void processFrame(const char* buf, int len, bool localAnswer = false, bool replayed = false) {
  static int32_t cntOTmessagesprocessed = 0;
  cntOTmessagesprocessed++;
  if (buf[0] == 'B') OTdata.rsptype = OTGW_BOILER;            // the source letter, as processOT() sets it
  else if (buf[0] == 'T') OTdata.rsptype = OTGW_THERMOSTAT;
  else if (buf[0] == 'R') OTdata.rsptype = OTGW_REQUEST_BOILER;
  else if (buf[0] == 'A') OTdata.rsptype = OTGW_ANSWER_THERMOSTAT;
@PARSE@
@PAIR@
      OTdata.skipthis = (OTdata.rsptype == OTGW_PARITY_ERROR);
@PMANSWER@
@FEEDHOOK@
    }
}

// ---- SAT ---------------------------------------------------------------------------------------------------------------------
struct SatSettingsStub { bool bSimulation = false; };
struct SettingsStub { SatSettingsStub sat; };
static SettingsStub settings;
static bool g_satOn = true, g_unsup25 = false;
static bool satIsEnabled() { return g_satOn; }
static bool hasOTCommandInterface() { return true; }
static bool isBoilerMsgIdUnsupportedRead(uint8_t id) { return id == 25 && g_unsup25; }
#define SATDebugTln(...)
@STEP@

// ---- bus model ---------------------------------------------------------------------------------------------------------------
static int  g_picPrio = 0;          // the PM= the PIC holds (its one priority slot), 0 = none
static bool g_picServes = true;     // the PIC uses a rejected-DataID request for its priority message
static char g_reply25 = 'v';        // the boiler's reply to the PIC's MsgID 25 request: v = Read-Ack 45.0, i = Data-Invalid
static bool g_aaList = false;       // the PIC's AA list holds 116 (supported): every rejected request is replaced (signature 1)
static uint8_t g_status = 0x00;     // MsgID 0 slave status low byte the boiler reports
static char g_log[8192];
static void f(char src, uint32_t v) { char b[12]; snprintf(b, sizeof(b), "%c%08X", src, (unsigned)v); processFrame(b, 9); }
static uint32_t RD(int id) { return (uint32_t)id << 16; }
static void thermostatRequest(int id, bool rejected) {
  if (id == 25) { f('T', RD(25)); f('B', 0x40000000u | RD(25) | 0x2D00); return; }
  if (id == 0)  { f('T', RD(0)); f('B', 0x40000000u | RD(0) | g_status); return; }
  if (!rejected) { f('T', RD(id)); f('B', 0x40000000u | RD(id) | 0x1400); return; }
  if (g_picPrio && g_picServes) {
    const int p = g_picPrio;
    f('T', RD(id)); f('R', RD(p));
    if (p == 25 && g_reply25 == 'i') f('B', 0x60000000u | RD(25));
    else f('B', 0x40000000u | RD(p) | 0x2D00);
    f('A', 0x70000000u | RD(id));
    g_picPrio = 0;                                     // HandleResponse: a reply with the priority DataID clears it
    return;
  }
  if (g_aaList) { f('T', RD(id)); f('R', RD(116)); f('B', 0x40000000u | RD(116) | 0x0010); f('A', 0x70000000u | RD(id)); return; }
  f('T', RD(id)); f('B', 0x70000000u | RD(id));         // nothing pending: sent unmodified, the boiler rejects it
}
static char g_sentLog[8192];
static int  g_pm25 = 0;
static void picDrain(uint32_t t) {   // what the queue sent this pass: the PIC echoes it and loads a PM= into its slot
  char line[64]; const char* p = g_sent;
  while (sscanf(p, "%63[^;];", line) == 1) {
    char e[40]; snprintf(e, sizeof(e), "%c%c: %s", line[0], line[1], line + 3);
    checkCommandResponse(e, (unsigned)strlen(e));
    if (line[0] == 'P' && line[1] == 'M') {
      g_picPrio = atoi(line + 3);
      if (g_picPrio == 25) { char ts[16]; snprintf(ts, sizeof(ts), "%lu,", (unsigned long)t); strcat(g_sentLog, ts); g_pm25++; }
    }
    p = strchr(p, ';') + 1;
  }
  g_sent[0] = 0;
}
// ids: the thermostat's request list, one per second; rej: which of them the boiler rejects
static const int* g_ids = nullptr; static int g_nIds = 0; static const bool* g_rej = nullptr;
static uint32_t g_t25Period = 0;    // > 0: the thermostat also reads MsgID 25 every g_t25Period ms
static void run(uint32_t fromMs, uint32_t toMs) {
  static int k = 0;
  for (uint32_t t = fromMs; t < toMs; t += 100) {
    g_ms = t;
    if (t % 1000 == 0) {
      if (g_t25Period && t % g_t25Period == 0) thermostatRequest(25, false);
      else if (g_nIds) { thermostatRequest(g_ids[k % g_nIds], g_rej[k % g_nIds]); k++; }
    }
#ifndef NO_PM25_STEP
    satPm25PollStep();
#endif
    handleCommandQueue();
    picDrain(t);
  }
}
static void report() {
  printf("pm25=%d\nsent_ms=%s\n", g_pm25, g_sentLog);
#ifndef NO_PM25_STEP
  printf("no_slot=%d\nn_sent=%u\nn_answered=%u\nn_unanswered=%u\n", (int)state.sat.bPm25NoSlot, (unsigned)state.sat.iPm25Sent,
         (unsigned)state.sat.iPm25Answered, (unsigned)state.sat.iPm25Unanswered);
#else
  printf("no_slot=0\nn_sent=0\nn_answered=0\nn_unanswered=0\n");
#endif
}
static const int kIds[] = { 0, 18, 17, 3 };            // status, a DataID the boiler rejects (18), two it supports
static const bool kRej[] = { false, true, false, false };
static const int kIdsOk[] = { 0, 17, 3 };              // only supported DataIDs: no slot
static const bool kRejOk[] = { false, false, false };
int main(int argc, char** argv) {
  const char* sc = argv[1];
  g_pic = true; g_ids = kIds; g_nIds = 4; g_rej = kRej;
  state.otBus.bGatewayMode = true; state.otBus.bGatewayModeKnown = true; state.sat.bActive = true;
  uint32_t until = argc > 2 ? (uint32_t)atol(argv[2]) : 420000;
  if (!strcmp(sc, "quiet")) { }                                         // 1: no T25, flow never seen, rejected DataID polled
  else if (!strcmp(sc, "quiet_aa")) { g_aaList = true; }               // 1b: same, but the PIC replaces every rejected request
  else if (!strcmp(sc, "t25")) { g_t25Period = 40000; }                // 2: the thermostat reads 25 every 40 s
  else if (!strcmp(sc, "noslot")) { g_ids = kIdsOk; g_nIds = 3; g_rej = kRejOk; }   // 3
  else if (!strcmp(sc, "ps")) { state.otBus.bPSmode = true; }          // 4
  else if (!strcmp(sc, "monitor")) { state.otBus.bGatewayMode = false; }
  else if (!strcmp(sc, "unsup")) { g_unsup25 = true; }
  else if (!strcmp(sc, "satoff")) { g_satOn = false; }
  else if (!strcmp(sc, "chain")) { g_picServes = false; run(1000, 250000); g_ms = 250000; pmChainStart(); run(250000, until); report(); return 0; }
  else if (!strcmp(sc, "datainvalid")) { g_reply25 = 'i'; }            // 7: the boiler answers Data-Invalid
  else if (!strcmp(sc, "unanswered")) { g_picServes = false; }         // 8: the PIC never serves the PM=25
  else if (!strcmp(sc, "fault")) {                                      // 6: fault edge at 296 s
    run(1000, 296000); g_status = 0x01; run(296000, until); report(); return 0;
  }
  else if (!strcmp(sc, "seq")) {                                        // 5: the four frames through the processOT pieces
    g_picServes = false;                                                // the slot is shown (T18 B18 Unknown) but not used yet
    run(1000, 301000);
    printf("before_pm25=%d\nprio_slot=%d\n", g_pm25, (int)s_prioSlotId);
    g_ms = 301000;
    f('T', 0x80120000u); f('R', 0x80190000u); f('B', 0xC0192D00u); f('A', 0xF0120000u);   // T 18, R 25, B 25 = 45.0, A 18
    g_picPrio = 0;
    printf("feed_valid=%d\nfeed_sample=%lu\n", (int)(state.otBus.feedTboiler.lastEvent == SatFeedEvent::Valid),
           (unsigned long)state.otBus.feedTboiler.sampleMs);
    g_ms = 301100; handleCommandQueue();
    printf("prio_after=%d\n", (int)s_prioSlotId);
    g_picServes = true;
    run(301100, until);
    report();
    return 0;
  }
  else return 2;
  run(1000, until);
  report();
  return 0;
}
"""


def build_source(core, sat, pm25_h, literal_slot=False):
    core = core.replace("\r\n", "\n")
    sat = sat.replace("\r\n", "\n")
    feed = _read(FEED_H)
    prefix = ""
    if pm25_h is not None:
        hdr = pm25_h.replace("\r\n", "\n").replace('#include "SATboilerFeed.h"\n', "")
        if literal_slot:   # the rule as the TASK-1653 notes wrote it: only T(X) -> R(Y != X) counts as a slot
            line = "  if (afterT && src == 'B' && prevId == id && type == SAT_PM25_OT_UNKNOWN) b.slotMs = satFeedStamp(nowMs);\n"
            assert line in hdr, "signature 2 line moved"
            hdr = hdr.replace(line, "")
    else:
        hdr = "struct SatPm25Bus {};\n"
        prefix += "#define NO_PM25_STEP\n"
    body = pmc.extract(core)
    if "const bool satOwn" in body:
        prefix += "#define HAS_SAT_OWN_PARAM\n"
    hdr_fw = _read(FW / "OTGW-firmware.h")
    a = hdr_fw.index("struct CmdQueueEchoStats")
    b = hdr_fw.index("void cmdQueueSnapshot(CmdQueueSnap* out);")
    types = hdr_fw[a:b].replace("extern CmdQueueEchoStats g_cmdQueueEcho;", "")
    m = re.search(r"^#define\s+PIC_CMD_MAX_LEN\s+(\d+)", _read(FW / "OTGW-Core.h"), re.M)
    head = pmc.STUBS.split("@BODY@")[0]
    state_new = ("struct OTBusStub { bool bPSmode = false, bGatewayMode = true, bGatewayModeKnown = true; SatBoilerFeed feedTboiler, feedTret;"
                 " SatPm25Bus pm25; };\n"
                 "struct SatStub { bool bActive = true, bPm25NoSlot = false; uint32_t iPm25Sent = 0, iPm25Answered = 0, iPm25Unanswered = 0; };\n"
                 "struct State { PicState pic; OTBusStub otBus; SatStub sat; };")
    assert "struct State { PicState pic; };" in head
    head = head.replace("struct State { PicState pic; };", state_new)
    head = head.replace("@QUEUE_TYPES@", types).replace("@PICMAX@", m.group(1) if m else "14")
    parse = _cut(core, "    const char *bufval = buf + 1;", "OTdata.bReplayed = replayed;")
    pair = _cut(core, "    if (cntOTmessagesprocessed == 1) {",
                "OTdata.bGatewaySubstituted = bGatewaySubstituted;  //flag substitution if needed (ADR-096)\n")
    pm_line = re.search(r"^.*pmChainOnBoilerResponse\(OTdata\.id\);.*\n", core, re.M).group(0)
    feed_hook = _cut(core, "      if ((OTdata.id == OT_Tboiler || OTdata.id == OT_Tret) && OTdata.masterslave == 1 && !OTdata.skipthis) {",
                     "\n      }\n")
    step = ""
    ms = re.search(r"^static uint32_t _sat_pm25SentMs.*?^static void satPm25PollStep\(\)\n\{.*?^\}\n", sat, re.S | re.M)
    if ms and pm25_h is not None:
        step = ms.group(0)
    elif pm25_h is not None:
        prefix += "#define NO_PM25_STEP\n"
    src = (prefix + "#include <cstdint>\n" + feed + "\n" + hdr + "\n" + head + body + HARNESS)
    for k, v in (("@PARSE@", parse), ("@PAIR@", pair), ("@PMANSWER@", pm_line), ("@FEEDHOOK@", feed_hook), ("@STEP@", step)):
        src = src.replace(k, v)
    return src


def _times(out):
    return [int(x) for x in out["sent_ms"].split(",") if x]


class _Base(unittest.TestCase):
    LITERAL = False

    @classmethod
    def setUpClass(cls):
        cls.dir = tempfile.TemporaryDirectory()
        d = Path(cls.dir.name)
        pm25 = PM25_H.read_text(encoding="utf-8") if PM25_H.exists() else None
        cpp = d / "t.cpp"
        cpp.write_text(build_source(CORE_INO.read_text(encoding="utf-8"), SAT_INO.read_text(encoding="utf-8"), pm25, cls.LITERAL),
                       encoding="utf-8")
        cls.exe = compile_cpp(cpp, d / "t.exe")

    @classmethod
    def tearDownClass(cls):
        cls.dir.cleanup()

    def run_sc(self, name, *args):
        if not self.exe:
            self.skipTest("no host C++ compiler")
        r = subprocess.run([str(self.exe), name, *args], capture_output=True, text=True)
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        return dict(line.split("=", 1) for line in r.stdout.splitlines())


class Pm25Poll(_Base):
    def test_1_quiet_thermostat_one_pm25(self):
        """No T25 for 300 s, flow never seen (age >= 20 s), a rejected DataID polled: exactly one PM=25, not before 300 s."""
        out = self.run_sc("quiet", "305000")
        print(f"\n  quiet to 305 s: PM=25 at {out['sent_ms']} ms, sent={out['n_sent']}")
        self.assertEqual(out["pm25"], "1", out)
        self.assertGreaterEqual(_times(out)[0], 300000)

    def test_1b_quiet_rate_and_answers(self):
        out = self.run_sc("quiet", "420000")
        t = _times(out)
        gaps = [b - a for a, b in zip(t, t[1:])]
        print(f"\n  quiet to 420 s: {len(t)} PM=25 at {t}, gaps {gaps}, answered={out['n_answered']}")
        self.assertGreaterEqual(len(t), 4)
        self.assertTrue(all(g >= 20000 for g in gaps), gaps)
        self.assertGreaterEqual(int(out["n_answered"]), int(out["n_sent"]) - 1, out)   # the last one may still be outstanding
        self.assertEqual(out["n_unanswered"], "0", out)
        self.assertEqual(out["no_slot"], "0")

    def test_1c_aa_list_slot_also_counts(self):
        out = self.run_sc("quiet_aa", "305000")
        print(f"\n  AA list (T18 R116): PM=25 at {out['sent_ms']}")
        self.assertEqual(out["pm25"], "1", out)

    def test_2_thermostat_reads_25_no_pm25(self):
        out = self.run_sc("t25", "600000")
        print(f"\n  thermostat reads 25 every 40 s, 600 s: PM=25 count {out['pm25']}")
        self.assertEqual(out["pm25"], "0", out)
        self.assertEqual(out["no_slot"], "0")

    def test_3_no_slot_no_pm25_status_set(self):
        out = self.run_sc("noslot", "420000")
        print(f"\n  only supported DataIDs: PM=25 count {out['pm25']}, no_slot={out['no_slot']}")
        self.assertEqual(out["pm25"], "0", out)
        self.assertEqual(out["no_slot"], "1", out)

    def test_4_gates(self):
        for sc in ("ps", "monitor", "unsup", "satoff", "chain"):
            with self.subTest(gate=sc):
                out = self.run_sc(sc, "420000")
                print(f"\n  gate {sc}: PM=25 count {out['pm25']}")
                self.assertEqual(out["pm25"], "0", out)

    def test_5_frame_sequence_through_processot(self):
        out = self.run_sc("seq", "345000")
        t = _times(out)
        print(f"\n  seq: before={out['before_pm25']} prio_slot={out['prio_slot']} feed_valid={out['feed_valid']} "
              f"sample={out['feed_sample']} prio_after={out['prio_after']} PM=25 at {t} answered={out['n_answered']}")
        self.assertEqual(out["before_pm25"], "1", out)        # SAT asked; the PIC holds it, the slot waits for 25
        self.assertEqual(out["prio_slot"], "25", out)
        self.assertEqual(out["feed_valid"], "1", out)         # B C0192D00 stamped the flow feed as a valid sample
        self.assertEqual(out["feed_sample"], "301000", out)
        self.assertEqual(out["prio_after"], "255", out)       # the queue freed the priority slot (PRIO_SLOT_FREE)
        self.assertGreaterEqual(int(out["n_answered"]), 1, out)
        self.assertGreaterEqual(t[1], 301000 + 20000, t)      # the next PM=25 waits for the flow sample to be 20 s old

    def test_6_fault_edge_holds_off(self):
        out = self.run_sc("fault", "330000")
        t = _times(out)
        print(f"\n  fault edge at 296 s: PM=25 at {t}")
        self.assertTrue(t, out)
        self.assertGreaterEqual(t[0], 296000 + 10000, t)

    def test_7_data_invalid_answer_keeps_spacing(self):
        out = self.run_sc("datainvalid", "420000")
        t = _times(out)
        gaps = [b - a for a, b in zip(t, t[1:])]
        print(f"\n  Data-Invalid answers: {len(t)} PM=25, gaps {gaps}")
        self.assertTrue(t, out)
        self.assertTrue(all(g >= 20000 for g in gaps), gaps)
        self.assertLessEqual(len(t), 7)

    def test_8_unanswered_sets_status(self):
        out = self.run_sc("unanswered", "430000")
        t = _times(out)
        gaps = [b - a for a, b in zip(t, t[1:])]
        print(f"\n  never served: PM=25 at {t}, gaps {gaps}, unanswered={out['n_unanswered']} no_slot={out['no_slot']}")
        self.assertGreaterEqual(len(t), 2, out)
        self.assertTrue(all(g >= 60000 for g in gaps), gaps)  # one outstanding until the 60 s slot timeout
        self.assertGreaterEqual(int(out["n_unanswered"]), 1, out)
        self.assertEqual(out["no_slot"], "1", out)


class LiteralSlotRule(_Base):
    """The rule as written in the TASK-1653 notes (only T X -> R Y != X): on a PIC with nothing pending it never fires."""
    LITERAL = True

    def test_literal_rule_never_fires_in_steady_state(self):
        if not PM25_H.exists():
            self.skipTest("no SATpm25Poll.h in this copy")
        out = self.run_sc("quiet", "420000")
        print(f"\n  literal slot rule, steady state (T18 B18 Unknown, no R): PM=25 count {out['pm25']}, no_slot={out['no_slot']}")
        self.assertEqual(out["pm25"], "0", out)
        self.assertEqual(out["no_slot"], "1", out)

    def test_literal_rule_fires_with_aa_list(self):
        if not PM25_H.exists():
            self.skipTest("no SATpm25Poll.h in this copy")
        out = self.run_sc("quiet_aa", "305000")
        self.assertEqual(out["pm25"], "1", out)


class SourceGuards(unittest.TestCase):
    """What the harness cannot execute: the wiring."""

    @classmethod
    def setUpClass(cls):
        cls.core = _read(CORE_INO)
        cls.sat = _read(SAT_INO)

    def test_processot_calls_the_hook_before_the_pairing(self):
        a = self.core.index("void processOT(const char *buf, int len")
        h = self.core.find("satPm25NoteFrame(state.otBus.pm25", a)
        p = self.core.find("bool bGatewaySubstituted = (delayedOTdata.id == OTdata.id)", a)
        self.assertTrue(0 < h < p, "processOT() must call satPm25NoteFrame() on the incoming frame, before the pairing")
        self.assertIn("if (!localAnswer && !replayed) {", self.core[h - 400:h])

    def test_control_loop_calls_the_step_after_the_gates(self):
        a = self.sat.index("if (satRestartStep()) return;")
        s = self.sat.find("  satPm25PollStep();", a)
        t = self.sat.find("if (!DUE(timerSATControl)) return;", a)
        self.assertTrue(0 < s < t, "satPm25PollStep() must run after the restart gate, every pass, before the tick")

    def test_status_and_topic(self):
        self.assertIn('je.field(F("pm25_no_slot"),', self.sat)
        for k in ("pm25_sent", "pm25_answered", "pm25_unanswered"):
            self.assertIn(f'je.field(F("{k}"),', self.sat)
        self.assertIn('publishIfChangedB(F("sat/pm25_no_slot"),', self.sat)
        self.assertIn("memset(&s_core_pm25_no_slot, 0,", self.sat)
        self.assertIn("`sat/pm25_no_slot`", _read(REPO / "docs" / "api" / "MQTT.md"))

    def test_entry45_flood_unchanged(self):
        self.assertIn('  } else if (!pmChainBusy()) {\n    addCommandToQueue("PM=25", 5, false, 0, true);\n  }', self.sat)


if __name__ == "__main__":
    unittest.main(verbosity=1)
