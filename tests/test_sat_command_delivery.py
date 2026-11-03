#!/usr/bin/env python3
"""TASK-1412.11 (LL-013): SAT command delivery is verifiable: intent, enqueue result, transmission and acknowledgement are separate events.

One host program holds BOTH real blocks, nothing is re-implemented:
  - the SAT command senders of src/OTGW-firmware/SATcontrol.ino (satSendCS/MM/CH, satEnqueueOwnCS, satPicCSKeepalive, satCmdResend,
    satResetCmdCache, satNotifyPicRestart, from SAT_CMD_REFRESH_MS up to the Thermal Drop section), and
  - the PIC command queue of src/OTGW-firmware/OTGW-Core.ino (addCommandToQueue, handleCommandQueue, checkCommandResponse, the
    priority slot, the PM chain), cut out the way tests/test_pic_pm_chain.py does it.
Only the SAT call to addCommandToQueue() goes through a logging wrapper (text replacement in the cut-out SAT source) so the enqueue result
is an event of its own. The stubs are a stub clock, sendPICSerial() (records the transmission), handleOTDirectCommand() (records the
call) and the PIC: an echo is fed to checkCommandResponse() a configurable time after the transmission, or not at all.

Event kinds in the log (EV <ms> <kind> <text>):
  INTENT       the logical decision (the control tick wanted this value)         harness line, the tick plays satControlLoopRun
  ENQ          the result of addCommandToQueue (Queued, Replaced, Sent, Dropped) real return value
  TX           the command left for the PIC (sendPICSerial)                      real queue code
  HANDLER      OT-Direct only: handleOTDirectCommand() was called with it        the OT frame itself goes later, see NOT VERIFIED
  PIC_ECHO     the PIC's echo/answer, fed to checkCommandResponse()              real queue code
  BOILER_ACK   a boiler frame for a DataID, fed to pmChainOnBoilerResponse()     the only boiler-answer consumer of the queue

Run: python tests/test_sat_command_delivery.py
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
import test_pic_pm_chain as pm  # noqa: E402

SAT_INO = Path(os.environ.get("OTGW_SAT_INO", REPO / "src" / "OTGW-firmware" / "SATcontrol.ino"))   # OTGW_CORE_INO: see test_pic_pm_chain

SAT_STUBS = r"""
#define SATTraceTf(...) ((void)0)
#define SATTraceTln(x) ((void)0)
#define SATDebugTf(...) ((void)0)
#define SATDebugTln(x) ((void)0)
#include <cmath>
static void satNarrate_P(const char*) {}
static void satNarratef_P(const char*, ...) {}
static bool _sat_followNarrPending = false;
static bool _sat_narrAtMax = false;
static bool _sat_narrCold = false;
static bool _sat_bootBoilerLogged = false;
static bool _sat_bootOutsideLogged = false;
static bool hasOTCommandInterface() { return true; }
static float satEffectiveMaxSetpoint() { return 1000.0f; }
static bool satImmergasTpActive() { return false; }
struct OTSys { uint8_t SlaveStatus = 0; }; static OTSys OTcurrentSystemState;
static int16_t satPreSatPersisted(bool) { return 0; }
static void satPreSatPersist(int16_t, int16_t) {}
static bool satIsEnabled() { return true; }
"""

SAT_STATE = ("struct SatState { bool bActive=true; bool bLastSentValid=false; float fLastSentCS=0; uint8_t iLastSentMM=0, iLastSentCH=0; float fLastSentTC=0;"
             " bool bTCAccountedValid=false; uint32_t iLastSentCSMs=0, iLastSentMMMs=0, iLastSentCHMs=0; uint32_t iTCPushMs=0; bool bPushRefused=false;"
             " uint8_t iShVerdict=0; };\n")

MAIN = r"""
// ---- event log -------------------------------------------------------------------------------------------------------------------------
static const char* rname(CmdQueueResult r) {
  switch (r) { case CmdQueueResult::Queued: return "Queued"; case CmdQueueResult::Replaced: return "Replaced";
               case CmdQueueResult::Sent: return "Sent"; default: return "Dropped"; }
}
static void ev(const char* kind, const char* text) { printf("EV %lu %s %s\n", (unsigned long)g_ms, kind, text); }
static void evf(const char* kind, const char* fmt, ...) {
  char b[96]; va_list a; va_start(a, fmt); vsnprintf(b, sizeof b, fmt, a); va_end(a); ev(kind, b);
}
// the wrapper the cut-out SAT source calls instead of addCommandToQueue() (text replacement), so the result is its own event
static CmdQueueResult addCmdLogged(const char* c, size_t n, bool force, int delay, bool own = false) {
  char t[40]; size_t k = n < 39 ? n : 39; memcpy(t, c, k); t[k] = 0;
  const size_t before = strlen(g_direct);
  const CmdQueueResult r = addCommandToQueue(c, (int)n, force, (int16_t)delay, own);
  ev("ENQ", (std::string(t) + " " + rname(r)).c_str());
  if (!g_pic && strlen(g_direct) > before) ev("HANDLER", t);
  return r;
}
// ordinary traffic (REST/MQTT/UI): logged the same way, not SAT's
static CmdQueueResult userCmd(const char* c) { return addCmdLogged(c, strlen(c), false, 0, false); }
static void intent(const char* what) { ev("INTENT", what); }

// ---- the PIC and the clock -----------------------------------------------------------------------------------------------------------
struct Pend { uint32_t at; char echo[40]; bool used; };
static Pend pend[64];
static uint32_t echoLat = 25;             // ms from transmission to echo (calculated ~21 ms at 9600 baud; TASK-1571)
static char muteCmd[16] = "";             // transmissions starting with this text get no echo (empty = all echoed)
static bool muteAll = false;
static void schedEcho(const char* line) {
  if (muteAll || (muteCmd[0] && !strncmp(line, muteCmd, strlen(muteCmd)))) return;
  for (auto& p : pend) if (!p.used) {
    p.used = true; p.at = g_ms + echoLat;
    snprintf(p.echo, sizeof p.echo, "%c%c: %s", line[0], line[1], line + 3); return;
  }
}
static bool simBoilerPrio = false;
static uint32_t prioAnswerAt = 0; static int prioAnswerId = -1;   // the simulated boiler answers a PM=/TP= 400 ms after it left
static void step(uint32_t t) {
  g_ms = t;
  if (prioAnswerId >= 0 && (int32_t)(t - prioAnswerAt) >= 0) {
    evf("BOILER_ACK", "DataID %d", prioAnswerId); pmChainOnBoilerResponse((uint8_t)prioAnswerId); prioAnswerId = -1;
  }
  for (auto& p : pend) if (p.used && (int32_t)(t - p.at) >= 0) {
    p.used = false; ev("PIC_ECHO", p.echo); checkCommandResponse(p.echo, (unsigned)strlen(p.echo));
  }
  g_sent[0] = 0; handleCommandQueue();
  char line[64]; const char* s = g_sent;
  while (sscanf(s, "%63[^;];", line) == 1) {
    ev("TX", line); schedEcho(line);
    if (simBoilerPrio && (!strncmp(line, "PM=", 3) || !strncmp(line, "TP=", 3))) { prioAnswerAt = t + 400; prioAnswerId = atoi(line + 3); }
    s = strchr(s, ';') + 1;
  }
}
static void run(uint32_t from, uint32_t to, uint32_t dt = 10) { for (uint32_t t = from; t <= to; t += dt) step(t); }
static void tick(float cs, int mm, int ch) {                  // satControlLoopRun's tick: CS, MM, CH
  char b[32]; snprintf(b, sizeof b, "CS=%.1f MM=%d CH=%d", cs, mm, ch); intent(b);
  satSendCS(cs); satSendMM((uint8_t)mm); satSendCH((uint8_t)ch);
}
static void qdump() { printf("Q size=%d", cmdQueueSize); for (int i = 0; i < cmdQueueSize; i++) printf(" [%s]", cmdqueue[i].cmd); printf("\n"); }

int main(int argc, char** argv) {
  const std::string sc = argv[1];
  const bool direct = argc > 2 && !strcmp(argv[2], "direct");
  g_pic = !direct; g_satOwns = true; g_inflight = false;
  g_ms = 100000; satResetCmdCache();

  if (sc == "flow") {              // one clean control tick, then the 5 s resend, the boiler answers later
    tick(40.0f, 80, 1);
    run(100000, 101000);
    ev("BOILER_ACK", "MsgID 1 write-ack");   // nothing in the firmware consumes it for CS/MM/CH
    printf("STATE last_cs=%.1f mm=%u ch=%u cs_ms=%lu\n", state.sat.fLastSentCS, state.sat.iLastSentMM, state.sat.iLastSentCH, (unsigned long)state.sat.iLastSentCSMs);
    qdump();
    run(101010, 106100);
    satCmdResend();
    run(106110, 106500);
    qdump();
  } else if (sc == "full") {       // 20 ordinary writes fill the queue; SAT's tick is refused; the queue drains; the NEXT tick carries the new value
    muteAll = true;
    char c[8];
    for (int k = 0; k < 20; k++) { snprintf(c, sizeof c, "%c%c=1", 'A' + k / 6, 'A' + k % 6); userCmd(c); }
    qdump();
    tick(40.0f, 80, 1);                                  // refused: CS, MM, CH all Dropped (the writes are not reads, nothing is evicted)
    printf("STATE after_drop valid=%d last_cs_ms=%lu mm_valid=%d\n", (int)state.sat.bLastSentValid, (unsigned long)state.sat.iLastSentCSMs, (int)_sat_mmSentValid);
    muteAll = false; echoLat = 25;
    run(100010, 104000);                                 // the ordinary writes drain with echo (no echo for the first: it was sent with muteAll)
    qdump();
    g_ms = 104000; tick(45.0f, 60, 1);                   // the latest approved state is now different: 45.0 / 60
    run(104010, 106000);
    qdump();
  } else if (sc == "superseded") { // an ON decision waits in the queue, a newer OFF decision replaces it: the ON never reaches the wire
    muteAll = true; userCmd("AA=1");                     // the PIC is busy with an earlier command, nothing is echoed yet
    run(100000, 100100);
    muteAll = false;
    intent("CS=40.0 CH=1"); satSendCS(40.0f); satSendCH(1);
    intent("CS=10.0 CH=0"); g_ms = 100200; satSendCS(10.0f); satSendCH(0);
    qdump();
    run(100210, 102000);
  } else if (sc == "interrupted") { // CS=40.0 on the wire, no echo (exchange interrupted); a newer CS=45.0 replaces it; a late echo of the old one arrives
    muteAll = true;
    intent("CS=40.0"); satSendCS(40.0f);
    run(100000, 100500);
    intent("CS=45.0"); g_ms = 100600; satSendCS(45.0f);
    muteAll = false;
    run(100610, 100700);
    const char* late = "CS: 40.0"; ev("PIC_ECHO", "CS: 40.0 (late, for the replaced command)"); checkCommandResponse(late, 8);
    qdump();
    run(100710, 112000);
    qdump();
  } else if (sc == "noecho5") {    // MM=80 is never echoed: 5 sends, then the queue drops it; SAT's cache says "sent"; the 60 s repair offers it again
    strcpy(muteCmd, "MM=");
    tick(40.0f, 80, 1);
    for (uint32_t t = 100000; t <= 230000; t += 100) {      // the control tick repeats every 60 s with the same values; resend every 5 s
      g_ms = t; satCmdResend();
      if (t > 100000 && (t - 100000) % 60000 == 0) tick(40.0f, 80, 1);
      step(t);
    }
    printf("STATE dropped=%u flag=%d queued=%d\n", (unsigned)state.pic.iCmdDropped, (int)state.pic.bCmdRetryExceeded, cmdQueueSize);
  } else if (sc == "reset") {      // steady state, then the PIC restarts: the exchange in flight is lost, the PIC forgot MM= and CH=
    tick(40.0f, 80, 1);
    run(100000, 101000);
    g_ms = 101010; tick(40.0f, 80, 1);                   // unchanged: only the CS goes (MM/CH are on-change)
    run(101010, 101500);
    muteAll = true;
    g_ms = 101600; intent("CS=42.0 (in flight when the PIC resets)"); satSendCS(42.0f);
    run(101600, 101700);
    ev("PIC_RESET", "banner"); satNotifyPicRestart();    // the same call Core makes on the PIC banner
    muteAll = false;
    g_ms = 101800; tick(43.0f, 70, 1);                   // the latest approved state at the time of the next tick
    run(101800, 106000);
    qdump();
  } else if (sc == "prio") {       // the PIC echo is a receipt, the boiler's answer frees the slot (PM= chain slot)
    intent("PM=3 then PM=15");
    userCmd("PM=3"); userCmd("PM=15");
    run(100000, 101500);
    ev("BOILER_ACK", "DataID 3"); pmChainOnBoilerResponse(3);
    run(101510, 103000);
  } else if (sc == "budget") {     // 600 s of SAT traffic with ordinary traffic; latency per SAT command is enqueue -> transmission
    // SAT loop in satControlLoopRun order every 100 ms: keepalive, suppression refresh (the real satEnqueueIfChangedCS every 3 s with a
    // value that changes each time = the worst case of entry 23; the decision logic of satPwmSuppressionRefresh() is NOT run, only its
    // cadence and its sender), resend, tick every 60 s. Ordinary traffic: PR= every 10 s, a burst of 8 writes every 120 s (REST/UI), a
    // PM= every 60 s that the simulated boiler answers 400 ms later (the PIC's priority slot is held until then).
    const uint32_t lat = argc > 3 ? (uint32_t)atoi(argv[3]) : 25; echoLat = lat; simBoilerPrio = true;
    float cs = 40.0f; int mm = 80; int sup = 0;
    for (uint32_t t = 100000; t <= 100000 + 600000; t += 10) {
      g_ms = t;
      if ((t - 100000) % 100 == 0) satPicCSKeepalive();
      if ((t - 100000) % 3000 == 1500) { sup ^= 1; satEnqueueIfChangedCS(31.0f + 0.5f * sup); }
      if ((t - 100000) % 100 == 0) satCmdResend();
      if ((t - 100000) % 60000 == 20000) userCmd("PM=7");
      if ((t - 100000) % 60000 == 0) { cs += 1.0f; mm = (mm == 80) ? 60 : 80; tick(cs, mm, 1); }
      if ((t - 100000) % 10000 == 5000) userCmd("PR=S");
      if ((t - 100000) % 120000 == 30000) { char c[8]; for (int k = 0; k < 8; k++) { snprintf(c, sizeof c, "L%c=%d", 'A' + k, k); userCmd(c); } }
      step(t);
    }
    qdump();
  } else if (sc == "deep") {       // CS behind 19 echoed ordinary writes (the worst case of TASK-1571 AC3), echo latency argv[3]
    echoLat = argc > 3 ? (uint32_t)atoi(argv[3]) : 25;
    char c[8];
    for (int k = 0; k < 19; k++) { snprintf(c, sizeof c, "%c%c=1", 'A' + k / 6, 'A' + k % 6); userCmd(c); }
    intent("CS=45.0"); satSendCS(45.0f);
    run(100000, 110000);
  } else {
    return 2;
  }
  return 0;
}
"""


def build_source() -> str:
    pre = pm.STUBS.split("@BODY@")[0]
    pre = pre.replace("struct State { PicState pic; };", SAT_STATE + "struct State { PicState pic; SatState sat; };")
    pre = "#include <string>\n#include <cstdarg>\n" + pre
    core = pm.extract(pm.CORE_INO.read_text(encoding="utf-8"))
    hdr = (REPO / "src" / "OTGW-firmware" / "OTGW-firmware.h").read_text(encoding="utf-8")
    a = hdr.index("struct CmdQueueEchoStats")
    b = hdr.index("void cmdQueueSnapshot(CmdQueueSnap* out);")
    types = hdr[a:b].replace("extern CmdQueueEchoStats g_cmdQueueEcho;", "")
    core_h = (REPO / "src" / "OTGW-firmware" / "OTGW-Core.h").read_text(encoding="utf-8")
    picmax = re.search(r"^#define\s+PIC_CMD_MAX_LEN\s+(\d+)", core_h, re.M).group(1)
    pre = pre.replace("@QUEUE_TYPES@", types).replace("@PICMAX@", picmax)
    sat_full = SAT_INO.read_text(encoding="utf-8").replace("\r\n", "\n")
    sat = sat_full[sat_full.index("static const uint32_t SAT_CMD_REFRESH_MS"):sat_full.index("// --- Thermal Drop Learning State")]
    sat = sat.replace("addCommandToQueue(", "addCmdLogged(")        # only the call sites: the result becomes an event
    # the part of the prelude before the Core body (prototypes), the Core body, the SAT stubs, the SAT body, the main
    pre_proto, _, _ = pre.partition("@BODY@")
    head, _, rest = MAIN.partition("// ---- the PIC and the clock")
    return "#define HAS_SAT_OWN_PARAM\n" + pre_proto + core + "\n" + SAT_STUBS + "\n" + head + sat + "\n// ---- the PIC and the clock" + rest


def parse(out: str):
    ev, other = [], []
    for line in out.splitlines():
        m = re.match(r"EV (\d+) (\S+) (.*)$", line)
        if m:
            ev.append((int(m.group(1)), m.group(2), m.group(3)))
        else:
            other.append(line)
    return ev, other


def kinds(ev, text, kind=None):
    return [(t, k, x) for (t, k, x) in ev if x.startswith(text) and (kind is None or k == kind)]


class CommandDelivery(unittest.TestCase):
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

    def sc(self, name, *args):
        if not self.exe:
            self.skipTest("no host C++ compiler")
        r = subprocess.run([str(self.exe), name, *args], capture_output=True, text=True)
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        return parse(r.stdout)

    # ---- AC1: the four stages are separate events --------------------------------------------------------------------------------------
    def test_pic_intent_enqueue_transmission_echo_are_separate_events_in_order(self):
        ev, other = self.sc("flow")
        for cmd, intent in (("CS=40.0", "CS=40.0 MM=80 CH=1"), ("MM=80", None), ("CH=1", None)):
            enq = kinds(ev, cmd, "ENQ")
            tx = kinds(ev, cmd, "TX")
            echo = [e for e in ev if e[1] == "PIC_ECHO" and e[2].startswith(cmd[:2] + ":")]
            self.assertTrue(enq and enq[0][2].endswith("Queued"), (cmd, enq))
            self.assertTrue(tx and echo, (cmd, tx, echo))
            self.assertLessEqual(enq[0][0], tx[0][0], cmd)
            self.assertGreater(echo[0][0], tx[0][0], f"{cmd}: the echo is a later event than the transmission")
            self.assertGreaterEqual(echo[0][0] - tx[0][0], 25, cmd)
        self.assertTrue(kinds(ev, "CS=40.0 MM=80 CH=1", "INTENT"))
        # intent and transmission are different events: the command sat in the queue between them
        self.assertTrue(any(k == "ENQ" for _, k, _ in ev) and any(k == "TX" for _, k, _ in ev))

    def test_pic_echo_is_a_receipt_not_boiler_acceptance(self):
        # PM=15 stays queued after PM=3 was echoed; only the boiler's frame for DataID 3 releases it (TASK-1572)
        ev, _ = self.sc("prio")
        tx15 = kinds(ev, "PM=15", "TX")
        echo3 = [e for e in ev if e[1] == "PIC_ECHO" and e[2] == "PM: 3"]
        ack = [e for e in ev if e[1] == "BOILER_ACK"]
        self.assertTrue(echo3 and ack and tx15, ev)
        self.assertLess(echo3[0][0], ack[0][0])
        self.assertGreaterEqual(tx15[0][0], ack[0][0], "PM=15 left before the boiler answered PM=3")

    def test_cs_mm_ch_have_no_boiler_ack_consumer_the_cache_is_set_at_enqueue(self):
        # The firmware keeps no 'accepted by the boiler' state for CS/MM/CH: the SAT cache is committed when the queue ACCEPTED the
        # command (commit-on-success, TASK-1574), before any transmission or echo. The boiler's answer (a B frame) is not read.
        ev, other = self.sc("flow")
        st = next(l for l in other if l.startswith("STATE"))
        cs_ms = int(re.search(r"cs_ms=(\d+)", st).group(1))
        first_tx = kinds(ev, "CS=40.0", "TX")[0][0]
        enq = kinds(ev, "CS=40.0", "ENQ")[0][0]
        self.assertEqual(cs_ms, enq, "the cache time is the enqueue time")
        self.assertLessEqual(cs_ms, first_tx)
        sat = SAT_INO.read_text(encoding="utf-8")
        a = sat.index("static const uint32_t SAT_CMD_REFRESH_MS")
        block = sat[a:sat.index("// --- Thermal Drop Learning State")]
        for needle in ("PIC_ECHO", "checkCommandResponse", "pmChainOnBoilerResponse", "bBoilerAck", "WRITE_ACK"):
            self.assertNotIn(needle, block, f"SAT command senders read {needle}")

    def test_ot_direct_enqueue_result_and_handler_call(self):
        ev, _ = self.sc("flow", "direct")
        for cmd in ("CS=40.0", "MM=80", "CH=1"):
            enq = kinds(ev, cmd, "ENQ")
            hnd = kinds(ev, cmd, "HANDLER")
            self.assertTrue(enq and enq[0][2].endswith("Sent"), (cmd, enq))
            self.assertTrue(hnd, cmd)
            self.assertFalse(kinds(ev, cmd, "TX"), "OT-Direct does not use the PIC wire")
            self.assertFalse([e for e in ev if e[1] == "PIC_ECHO"], "no PIC, no PIC echo")

    # ---- AC2: full queue, interrupted exchange, reset, superseded, recovery ----------------------------------------------------------------
    def test_full_queue_rejects_and_the_rejected_state_is_never_cached_as_delivered(self):
        ev, other = self.sc("full")
        enq = [e for e in ev if e[1] == "ENQ" and e[0] == 100000 and e[2].split()[0] in ("CS=40.0", "MM=80", "CH=1")]
        self.assertEqual([e[2].split()[1] for e in enq], ["Dropped"] * 3, enq)
        st = next(l for l in other if l.startswith("STATE after_drop"))
        self.assertIn("valid=0", st)     # nothing was recorded as sent
        self.assertIn("mm_valid=0", st)
        self.assertFalse(kinds(ev, "CS=40.0", "TX"), "a refused CS=40.0 must never reach the wire")
        self.assertFalse(kinds(ev, "MM=80", "TX"))
        # recovery: the next tick carries the LATEST approved state (45.0 / 60), not the refused 40.0 / 80
        self.assertTrue(kinds(ev, "CS=45.0", "TX"), ev[-12:])
        self.assertTrue(kinds(ev, "MM=60", "TX"))
        self.assertTrue(kinds(ev, "CH=1", "TX"))

    def test_superseded_command_never_reaches_the_wire(self):
        ev, other = self.sc("superseded")
        self.assertEqual([e[2] for e in ev if e[1] == "ENQ" and e[2].startswith(("CS=", "CH="))],
                         ["CS=40.0 Queued", "CH=1 Queued", "CS=10.0 Replaced", "CH=0 Replaced"])
        tx = [e[2] for e in ev if e[1] == "TX"]
        self.assertNotIn("CS=40.0", tx)
        self.assertNotIn("CH=1", tx)
        self.assertIn("CS=10.0", tx)
        self.assertIn("CH=0", tx)

    def test_interrupted_exchange_resends_the_newest_value_and_a_late_echo_does_not_cancel_it(self):
        ev, other = self.sc("interrupted")
        tx = [(t, x) for t, k, x in ev if k == "TX" and x.startswith("CS=")]
        self.assertEqual(tx[0][1], "CS=40.0")
        self.assertEqual([x for _, x in tx[1:2]], ["CS=45.0"], tx)
        # the late echo of the replaced CS=40.0 left the queued CS=45.0 in place (it is the first Q dump)
        q = [l for l in other if l.startswith("Q size=")]
        self.assertIn("[CS=45.0]", q[0])

    def test_unechoed_command_is_dropped_after_five_sends_and_repaired_within_60_s(self):
        # MM=80 is never echoed: the queue sends it 5 times (OTGW_CMD_RETRY) 5 s apart and drops it (command_retry_exceeded). SAT's cache
        # holds it as sent (the enqueue was accepted); the one-minute repair of entry 239 offers it again at the first control tick
        # that is 60 s old (here the tick runs every 60 s), so the boiler is not left without it. The CS= stream keeps running.
        ev, other = self.sc("noecho5")
        mm = [t for t, k, x in ev if k == "TX" and x == "MM=80"]
        self.assertGreaterEqual(len(mm), 6, mm)
        first5 = mm[:5]
        self.assertTrue(all(4900 <= b - a <= 6100 for a, b in zip(first5, first5[1:])), first5)
        gap = mm[5] - mm[4]
        self.assertGreaterEqual(mm[5] - mm[0], 60000, "repaired no earlier than 60 s after the accepted enqueue")
        self.assertLessEqual(mm[5] - mm[0], 62000, f"repaired late: {mm}")
        st = next(l for l in other if l.startswith("STATE"))
        self.assertRegex(st, r"dropped=[1-9]\d* ")   # each give-up is counted; the latched flag is cleared again by the next confirmed (echoed) CS=
        print(f"\n  unechoed MM=80: sends at +{[(t - mm[0]) // 1000 for t in mm]} s, repair {gap} ms after the fifth send")

    def test_pic_reset_resends_all_three_with_the_latest_values(self):
        ev, other = self.sc("reset")
        after = [e for e in ev if e[0] >= 101800 and e[1] == "TX"]
        names = [e[2] for e in after]
        self.assertIn("CS=43.0", names)       # the latest, not the lost CS=42.0 nor the older 40.0
        self.assertIn("MM=70", names)
        self.assertIn("CH=1", names)          # unchanged value, sent again because the PIC forgot it (satNotifyPicRestart)
        self.assertNotIn("CS=42.0", names)
        # before the reset, an unchanged tick sent only the CS: MM/CH are on change (entry 237)
        mid = [e[2] for e in ev if 101010 <= e[0] < 101600 and e[1] == "ENQ"]
        self.assertEqual([m for m in mid if m.startswith(("MM=", "CH="))], [], mid)

    # ---- AC3: budgets on the host, echo paced ----------------------------------------------------------------------------------------------
    def latencies(self, ev):
        """enqueue-accepted -> transmission, per SAT command text, over the whole log; the queue order is FIFO per command text."""
        pend = {}
        out = []
        for t, k, x in ev:
            if k == "ENQ" and not x.endswith("Dropped") and x.split()[0][:3] in ("CS=", "MM=", "CH="):
                pend[x.split()[0]] = t
            elif k == "TX" and x in pend:
                out.append((x, t - pend.pop(x)))
        return out

    def test_budget_sat_commands_under_ordinary_traffic_at_echo_pacing(self):
        res = {}
        for lat in (25, 200):
            ev, other = self.sc("budget", "pic", str(lat))
            lats = self.latencies(ev)
            self.assertGreater(len(lats), 100)
            worst = max(l for _, l in lats)
            cs_tx = [t for t, k, x in ev if k == "TX" and x.startswith("CS=")]
            gaps = [b - a for a, b in zip(cs_tx, cs_tx[1:])]
            res[lat] = (worst, max(gaps), len(cs_tx), max(l for x, l in lats if x.startswith("CS=")))
            # recorded bounds: a command behind a full echoing queue < 2 s (TASK-1571 AC3), CS repeat well inside the PIC's 63 s (ADR-203)
            self.assertGreaterEqual(len(kinds(ev, "PM=7", "TX")), 9, "ordinary priority traffic ran")
            self.assertGreaterEqual(len([e for e in ev if e[1] == "BOILER_ACK"]), 9)
            self.assertLess(worst, 2000, f"echo {lat} ms: slowest SAT command waited {worst} ms")
            self.assertLess(max(gaps), 5000 + 2000, f"echo {lat} ms: two CS= transmissions {max(gaps)} ms apart")
            self.assertLess(max(gaps), 63000)
            q = [l for l in other if l.startswith("Q size=")][-1]
        print("\n  budget (600 s, echo 25 ms / 200 ms): worst SAT command queue latency %d / %d ms, worst CS gap %d / %d ms, CS sends %d / %d"
              % (res[25][0], res[200][0], res[25][1], res[200][1], res[25][2], res[200][2]))

    def test_budget_cs_behind_a_full_echoing_queue(self):
        for lat in (25, 200):
            ev, _ = self.sc("deep", "pic", str(lat))
            tx = kinds(ev, "CS=45.0", "TX")
            self.assertTrue(tx, lat)
            wait = tx[0][0] - 100000
            print(f"\n  CS= behind 19 echoed writes, echo {lat} ms: transmitted after {wait} ms")
            if lat == 25:
                self.assertLess(wait, 2000, "TASK-1571 AC3 recorded bound")
            else:   # a slow PIC (200 ms echo, a guess) breaks the 2 s bound; the CS is still ~13x inside the PIC's ~63 s CS expiry
                self.assertGreater(wait, 2000)
                self.assertLess(wait, 10000)

    def test_ot_direct_budget_is_synchronous(self):
        ev, _ = self.sc("flow", "direct")
        enq = kinds(ev, "CS=40.0", "ENQ")[0][0]
        hnd = kinds(ev, "CS=40.0", "HANDLER")[0][0]
        self.assertEqual(hnd, enq, "OT-Direct hands the command to the handler within the enqueue call")


def ot_direct_refusal_reaches_the_caller() -> bool:
    """True when addCommandToQueue() can answer Dropped for an OT-Direct write that the handler refused (ring full, NG, BV).
    Read from the source (structural): OTDirect.ino's handleOTDirectCommand() is a 700-line function that is not cut out on the host."""
    core = (REPO / "src" / "OTGW-firmware" / "OTGW-Core.ino").read_text(encoding="utf-8")
    otd = (REPO / "src" / "OTGW-firmware" / "OTDirect.ino").read_text(encoding="utf-8")
    branch = core[core.index("CmdQueueResult addCommandToQueue("):]
    branch = branch[:branch.index("constexpr int kMaxCmdLen")]
    handler_status = re.search(r"^(bool|int|CmdQueueResult)\s+handleOTDirectCommand\(", otd, re.M) is not None
    return handler_status and "Dropped" in branch.split("handleOTDirectCommand")[1]


class OtDirectKnownGap(unittest.TestCase):
    """OT-Direct: addCommandToQueue() answers Sent although the handler may have refused the write (ring full). Open: see the report."""

    @unittest.expectedFailure
    def test_handler_refusal_is_visible_to_the_caller(self):
        self.assertTrue(ot_direct_refusal_reaches_the_caller(),
                        "handleOTDirectCommand() returns void and addCommandToQueue() returns Sent, so a write the OT-Direct ring refused is reported as delivered")


if __name__ == "__main__":
    unittest.main(verbosity=2)
