#!/usr/bin/env python3
"""TASK-1812 (ADR-241, Proposed): in master mode the thermostat's writes go to the boiler and the thermostat gets the
boiler's own answer, like the PIC.

Before: handleMasterModeSlaveFrame() answered every thermostat WRITE_DATA with a local WRITE_ACK echo and acted only on
MsgID 1, 16 and 24; a DHW setpoint (56) or max CH setpoint (57) write never reached the boiler. Now a WRITE_DATA of any
ID except 1, 16 and 24 is relayed in the next free master slot (MsgID 8 and 14 stay local while SAT owns the setpoint,
nothing while the boiler is offline), through the gateway-mode override rule, and the boiler's answer is returned when
it belongs to the current thermostat request and comes within the reply deadline counted from the request's arrival.

REAL code, cut from src/OTGW-firmware/OTDirect.ino and compiled on the host: otdMasterRelaysWrite() (markers),
applyOverrides(), the master-mode relay branch of loopOTDirect() and the TASK-1812 reply check of handleMasterResponse();
in a second program the real scheduleMasterRequest() with the prelude of tests/test_otd_mode_switch_queue_reset.py.
FAKE: the bus (sendMasterRequestAsync records), replyToThermostat (records), time. On the code before TASK-1812 the
relay branch and the reply check do not exist: setUpClass fails (red).
Run: python tests/test_otd_master_write_relay.py
"""
import os
import re
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

TESTS = Path(__file__).resolve().parent
sys.path.insert(0, str(TESTS))
from test_sat_pwm_override import compile_cpp  # noqa: E402
import test_otd_mode_switch_queue_reset as msq  # noqa: E402

OTD_INO = Path(os.environ.get("OTGW_OTD_INO", msq.REPO / "src" / "OTGW-firmware" / "OTDirect.ino"))


def braced_from(src, start):
    """The text from start to the brace that closes the first '{' after it, skipping char/string literals."""
    i = src.index("{", start)
    depth = 0
    while i < len(src):
        c = src[i]
        if c in "'\"":
            j = i + 1
            while src[j] != c:
                j += 2 if src[j] == "\\" else 1
            i = j + 1
            continue
        if src.startswith("//", i):
            i = src.index("\n", i)
            continue
        depth += {"{": 1, "}": -1}.get(c, 0)
        if depth == 0:
            return src[start:i + 1]
        i += 1
    raise AssertionError("unbalanced braces")


def parts(src):
    relay_head = "if (otSlaveFramePending && IS_MASTER_MODE() && otdMasterRelaysWrite(otSlaveFrame)) {"
    a = src.index("// --- otdMasterRelaysWrite begin")
    relays = src[a:src.index("// --- otdMasterRelaysWrite end")]
    apply = braced_from(src, src.index("static unsigned long applyOverrides(unsigned long frame, bool &modified) {"))
    branch = braced_from(src, src.index(relay_head))
    r0 = src.index("  // TASK-1812 (ADR-241): in master mode a relayed write answers only")
    reply = src[r0:src.index("\n  otMasterRequestActive = false;", r0)]
    deadline = re.search(r"static constexpr uint32_t OTD_REPLY_DEADLINE_MS = \d+;", src).group(0)
    deadline += "\n" + re.search(r"static constexpr uint32_t OTD_MWT_MS = \d+;", src).group(0)
    return relays, apply, branch, reply, deadline


PRE = r"""
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#define F(s) (s)
#define PSTR(s) (s)
#define OTDDebugTln(s) ((void)0)
#define OTDDebugTf(...) ((void)0)
enum OTDirectMode { OTD_MODE_BYPASS = 0, OTD_MODE_GATEWAY = 1, OTD_MODE_MONITOR = 2, OTD_MODE_MASTER = 3, OTD_MODE_LOOPBACK = 4 };
static OTDirectMode otCurrentMode = OTD_MODE_MASTER;
#define IS_MASTER_MODE() (otCurrentMode == OTD_MODE_MASTER)
enum OTDirectRequestOrigin : uint8_t { OT_DIRECT_ORIGIN_GATEWAY = 0, OT_DIRECT_ORIGIN_THERMOSTAT,
                                       OT_DIRECT_ORIGIN_THERMOSTAT_OVERRIDDEN };
enum class OpenThermResponseStatus : uint8_t { NONE, SUCCESS, INVALID, TIMEOUT };
struct { struct { bool bOnline = true; } otBus; } state;
static bool g_sat = false;
static bool satOwnsControlSetpoint() { return g_sat; }
static uint32_t g_now = 1000;
static uint32_t millis() { return g_now; }
@DEADLINE@
static bool otSlaveFramePending = false, otSlaveFrameNoted = false, otMasterRequestActive = false;
static unsigned long otSlaveFrame = 0, otLastThermostatRequest = 0, otLastSentRequest = 0;
static uint32_t otdSlaveFrameSeq = 0, otdRelaySeq = 0, otdRelaySentMs = 0, otdReplySinceMs = 0, otdLastConvEndMs = 0;
static bool otdReplyPending = false;
static OTDirectRequestOrigin otLastRequestOrigin = OT_DIRECT_ORIGIN_GATEWAY;
struct OTFrameOverride { uint8_t msgId; bool active; uint16_t overrideValue; };
static OTFrameOverride otOverrides[] = { { 1, false, 0 }, { 56, false, 0 }, { 57, false, 0 } };
static constexpr uint8_t OT_OVERRIDE_COUNT = sizeof(otOverrides) / sizeof(otOverrides[0]);
static void setOTParityBit(unsigned long&) {}
static unsigned long otdRelayedStatusFrame(unsigned long f) { return f; }
static int g_sent = 0; static unsigned long g_sentFrame = 0; static int g_sentOrigin = -1;
static bool sendMasterRequestAsync(unsigned long f, OTDirectRequestOrigin o) {
  g_sent++; g_sentFrame = f; g_sentOrigin = (int)o; otMasterRequestActive = true; otLastSentRequest = f; otLastRequestOrigin = o;
  return true;
}
static int g_replies = 0; static unsigned long g_reply = 0;
static void replyToThermostat(OTDirectRequestOrigin o, unsigned long, unsigned long resp) {
  if (o == OT_DIRECT_ORIGIN_GATEWAY) return;
  g_replies++; g_reply = resp;
}
static int g_local = 0;
// TASK-1906 / TASK-1803: applyOverrides() also rewrites some thermostat READs (MsgID 99, 27/56/57); this test relays
// WRITEs only, so those rewrites never apply here
static bool otdRelayRead99(unsigned long, unsigned long &) { return false; }
static bool otdRelayRewriteRead(unsigned long, unsigned long &) { return false; }
"""

MAIN = r"""
static void relayPass() {
  @BRANCH@
  else if (otSlaveFramePending) { g_local++; otSlaveFramePending = false; }   // the local handler path (today's echo)
}
static void responsePass(OpenThermResponseStatus status, unsigned long response) {
  const bool boilerDataInvalid = false;
  @REPLY@
  otMasterRequestActive = false;
  otLastRequestOrigin = OT_DIRECT_ORIGIN_GATEWAY;
}
static unsigned long W(uint8_t id, uint16_t v) { return (1UL << 28) | ((unsigned long)id << 16) | v; }
static void thermostat(unsigned long f) { otdSlaveFrameSeq++; otSlaveFrame = f; otSlaveFramePending = true; otSlaveFrameNoted = false; }
// argv[1] = scenario
int main(int argc, char** argv) {
  const char* s = argv[1];
  if (!strcmp(s, "w56")) {               // relay, then the boiler's WRITE_ACK goes back
    thermostat(W(56, 0x3C00)); relayPass();
    g_now += 100; responsePass(OpenThermResponseStatus::SUCCESS, (5UL << 28) | (56UL << 16) | 0x3C00);
  } else if (!strcmp(s, "local")) {      // 1, 16, 24 stay local
    thermostat(W(1, 0x2800)); relayPass(); thermostat(W(16, 0x1500)); relayPass(); thermostat(W(24, 0x1400)); relayPass();
  } else if (!strcmp(s, "sat")) {        // SAT owns: 8 and 14 local, 57 relayed
    g_sat = true; thermostat(W(14, 0x6400)); relayPass(); thermostat(W(8, 0x2800)); relayPass(); thermostat(W(57, 0x5000)); relayPass();
  } else if (!strcmp(s, "nosat14")) {    // without SAT 14 is relayed
    thermostat(W(14, 0x6400)); relayPass();
  } else if (!strcmp(s, "offline")) {    // boiler offline: nothing relayed (the local handler gives no answer, TASK-1807)
    state.otBus.bOnline = false; thermostat(W(56, 0x3C00)); relayPass();
  } else if (!strcmp(s, "override")) {   // SW=50 in force: the boiler gets 50, the reply is built from the thermostat's frame
    otOverrides[1].active = true; otOverrides[1].overrideValue = 0x3200; thermostat(W(56, 0x3C00)); relayPass();
  } else if (!strcmp(s, "busy")) {       // an own frame in flight: the write waits for its end, then the MWT, then goes
    otMasterRequestActive = true; thermostat(W(56, 0x3C00)); relayPass(); const int whileBusy = g_sent;
    g_now += 150; otMasterRequestActive = false; otdLastConvEndMs = g_now; relayPass(); const int inMwt = g_sent;
    g_now += 99; relayPass(); const int mwtMinus1 = g_sent;
    g_now += 1; relayPass();
    printf("whilebusy=%d inmwt=%d mwtminus1=%d sentms=%u ", whileBusy, inMwt, mwtMinus1, (unsigned)otdRelaySentMs);
  } else if (!strcmp(s, "intime")) {     // 600 ms behind an own frame, then answered 300 ms after the relay: sent
    otMasterRequestActive = true; thermostat(W(56, 0x3C00)); relayPass(); g_now += 500; otMasterRequestActive = false;
    otdLastConvEndMs = g_now; g_now += 100; relayPass();
    g_now += 300; responsePass(OpenThermResponseStatus::SUCCESS, (5UL << 28) | (56UL << 16) | 0x3C00);
  } else if (!strcmp(s, "satsh")) {      // SAT's SH= (override 57 in force) wins over the thermostat's 57 write
    g_sat = true; otOverrides[2].active = true; otOverrides[2].overrideValue = 0x4600; thermostat(W(57, 0x5000)); relayPass();
  } else if (!strcmp(s, "late")) {       // the answer comes 800 ms after the relay's send: past the deadline
    thermostat(W(56, 0x3C00)); relayPass();
    g_now += 800; responsePass(OpenThermResponseStatus::SUCCESS, (5UL << 28) | (56UL << 16) | 0x3C00);
  } else if (!strcmp(s, "reads")) {      // reads stay local: answered from the boiler cache (ADR-066), no boiler frame
    const unsigned long R0 = 0UL, R25 = 25UL << 16, R56 = 56UL << 16, R57 = 57UL << 16;
    thermostat(R0); relayPass(); thermostat(R25); relayPass(); thermostat(R56); relayPass(); thermostat(R57); relayPass();
  } else if (!strcmp(s, "newer")) {      // a newer thermostat frame arrived: the old answer is not sent
    thermostat(W(56, 0x3C00)); relayPass(); g_now += 50; otdSlaveFrameSeq++;
    g_now += 50; responsePass(OpenThermResponseStatus::SUCCESS, (5UL << 28) | (56UL << 16) | 0x3C00);
  }
  printf("sent=%d id=%lu data=%04lX origin=%d local=%d replies=%d reply=%04lX\n", g_sent, (g_sentFrame >> 16) & 0xFF,
         g_sentFrame & 0xFFFF, g_sentOrigin, g_local, g_replies, g_reply & 0xFFFF);
  return 0;
}
"""

SCHED_MAIN = r"""
int main(int argc, char** argv) {
  otCurrentMode = OTD_MODE_MASTER;
  for (int i = 0; i < OT_SCHEDULE_SIZE; i++) otSchedule[i] = { 0, 60000, 0, true, false, false, 0 };
  otSchedule[0] = { 25, 1000, 0, false, false, false, 0 };   // an own read that is due
  g_now = 100000;
  otSlaveFramePending = true;
  otSlaveFrame = argv[1][0] == 'w' ? ((1UL << 28) | (56UL << 16) | 0x3C00) : (25UL << 16);
  scheduleMasterRequest();
  printf("sent=%d\n", g_sent);
  return 0;
}
"""


class MasterWriteRelay(unittest.TestCase):
    tmp = None
    exe = None
    sched = None

    @classmethod
    def setUpClass(cls):
        src = OTD_INO.read_text(encoding="utf-8").replace("\r\n", "\n")
        relays, apply, branch, reply, deadline = parts(src)
        cls.tmp = tempfile.mkdtemp(prefix="otd_master_relay_")
        cpp = Path(cls.tmp) / "t.cpp"
        cpp.write_text(PRE.replace("@DEADLINE@", deadline) + relays + "\n" + apply + "\n" +
                       MAIN.replace("@BRANCH@", branch).replace("@REPLY@", reply), encoding="utf-8")
        cls.exe = compile_cpp(cpp, Path(cls.tmp) / ("t.exe" if os.name == "nt" else "t"))
        # mutant: the predicate without its WRITE_DATA check, to show the read case catches a relayed read
        guard = "((frame >> 28) & 0x07) != 1 /* WRITE_DATA */ ||"
        cls.mut = None
        if guard in relays:
            mcpp = Path(cls.tmp) / "m.cpp"
            mcpp.write_text(PRE.replace("@DEADLINE@", deadline) + relays.replace(guard, "") + "\n" + apply + "\n" +
                            MAIN.replace("@BRANCH@", branch).replace("@REPLY@", reply), encoding="utf-8")
            cls.mut = compile_cpp(mcpp, Path(cls.tmp) / ("m.exe" if os.name == "nt" else "m"))
        # the real scheduler with the real relay predicate
        stub = "static bool otdMasterRelaysWrite(unsigned long) { return false; }"
        pre = msq.PRELUDE.replace(stub, "static bool satOwnsControlSetpoint() { return false; }\n" + relays)
        pre = pre.replace('printf("  sent 0x%08lX\\n", (unsigned long)request);', "")
        ring_at = src.index("static constexpr uint8_t OT_CMD_QUEUE_SIZE")
        ring = src[ring_at:src.index("// TASK-494: enqueue with coalesce-by-MsgID.", ring_at)]
        note = (msq.cut_braced(src, "static void otdNoteCmdSent(uint32_t frame, uint32_t now)")
                if "static void otdNoteCmdSent(" in src else "")
        sched_cpp = Path(cls.tmp) / "s.cpp"
        sched_cpp.write_text(pre + ring + "\n" + note + "\n" + msq.cut_braced(src, "static void scheduleMasterRequest()") +
                             "\n" + SCHED_MAIN, encoding="utf-8")
        cls.sched = compile_cpp(sched_cpp, Path(cls.tmp) / ("s.exe" if os.name == "nt" else "s"))

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.tmp, ignore_errors=True)

    def run_case(self, exe, case):
        if not exe:
            self.skipTest("no host compiler")
        out = subprocess.run([str(exe), case], capture_output=True, text=True, check=True).stdout
        print(case, out.strip())
        return dict(kv.split("=") for kv in out.split())

    def test_a_dhw_setpoint_write_reaches_the_boiler_and_its_answer_goes_back(self):
        r = self.run_case(self.exe, "w56")
        self.assertEqual((r["sent"], r["id"], r["data"], r["origin"], r["local"]), ("1", "56", "3C00", "1", "0"))
        self.assertEqual((r["replies"], r["reply"]), ("1", "3C00"), "the boiler's WRITE_ACK goes back, not a local echo")

    def test_msgid_1_16_24_keep_the_local_path(self):
        r = self.run_case(self.exe, "local")
        self.assertEqual((r["sent"], r["local"]), ("0", "3"))

    def test_sat_owned_8_and_14_stay_local_while_57_is_relayed(self):
        r = self.run_case(self.exe, "sat")
        self.assertEqual((r["sent"], r["id"], r["local"]), ("1", "57", "2"))
        self.assertEqual((self.run_case(self.exe, "nosat14")["sent"]), "1", "without SAT a 14 write is relayed")

    def test_nothing_is_relayed_while_the_boiler_is_offline(self):
        r = self.run_case(self.exe, "offline")
        self.assertEqual((r["sent"], r["local"]), ("0", "1"))

    def test_a_user_override_replaces_the_data(self):
        r = self.run_case(self.exe, "override")
        self.assertEqual((r["data"], r["origin"]), ("3200", "2"), "SW=50 wins; origin THERMOSTAT_OVERRIDDEN")

    def test_a_write_waits_for_the_frame_in_flight_and_then_the_mwt(self):
        # ADR-241 F3: the master wait time, 100 ms from the end of the previous conversation (spec v4.2 :1100)
        r = self.run_case(self.exe, "busy")
        self.assertEqual((r["whilebusy"], r["inmwt"], r["mwtminus1"], r["sent"]), ("0", "0", "0", "1"))
        self.assertEqual(r["sentms"], "1250", "sent exactly when the MWT has passed (1000 + 150 + 100)")

    def test_an_answer_inside_the_deadline_from_the_relay_send_is_sent(self):
        # ADR-241 F2: 600 ms behind an own frame + 300 ms boiler answer = 900 ms after the arrival, 300 ms after the send
        self.assertEqual(self.run_case(self.exe, "intime")["replies"], "1")

    def test_an_answer_after_the_deadline_from_the_relay_send_is_not_sent(self):
        self.assertEqual(self.run_case(self.exe, "late")["replies"], "0")

    def test_sat_sh_override_wins_over_a_thermostat_max_ch_write(self):
        # ADR-241 F1: SAT sends SH= through enqueueWriteCommand(), which sets the 57 override; the boiler gets SAT's value
        r = self.run_case(self.exe, "satsh")
        self.assertEqual((r["sent"], r["id"], r["data"], r["origin"]), ("1", "57", "4600", "2"))

    def test_a_thermostat_read_in_master_mode_is_answered_from_the_cache_not_relayed(self):
        # regression guard (TASK-1812 bench brief): a READ_DATA of any ID takes the local handler path
        # (handleMasterModeSlaveFrame, answered from otBoilerCache, ADR-066); no master frame goes out for it
        r = self.run_case(self.exe, "reads")
        self.assertEqual((r["sent"], r["local"]), ("0", "4"))
        if self.mut:
            m = self.run_case(self.mut, "reads")
            self.assertNotEqual(m["sent"], "0", "the mutant without the WRITE_DATA check relays reads: the case sees it")

    def test_an_answer_for_an_older_request_is_not_sent(self):
        self.assertEqual(self.run_case(self.exe, "newer")["replies"], "0")

    def test_the_scheduler_waits_while_a_thermostat_write_waits(self):
        self.assertEqual(self.run_case(self.sched, "w")["sent"], "0", "the waiting write goes before the schedule")
        self.assertEqual(self.run_case(self.sched, "r")["sent"], "1", "a pending read does not hold the schedule")


if __name__ == "__main__":
    unittest.main(verbosity=2)
