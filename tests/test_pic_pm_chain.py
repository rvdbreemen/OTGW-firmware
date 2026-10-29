#!/usr/bin/env python3
"""TASK-1315: host test for the PIC priority-message (PM=) queue.

The real code is cut out of src/OTGW-firmware/OTGW-Core.ino (from the "Command Queue
implementation" header up to sendPICSerial(): removeFromCmdQueue(), addCommandToQueue(),
handleCommandQueue(), checkCommandResponse() and, on the fix, the PM chain) and compiled with
a host C++ compiler; nothing is re-implemented. sendPICSerial() is a stub that records the
lines, millis() is a stub clock. OTGW_CORE_INO points at another copy (e.g. the output of
`git show HEAD:src/OTGW-firmware/OTGW-Core.ino`) for old-vs-fix runs. Without a compiler the
tests are skipped.

Old behaviour (HEAD: only test_sat_init_asks_all_three is a genuine red, the chain-only scenarios skip there): SAT queued PM=3, PM=15, PM=48 with forceQueue=false, the prefix dedup
collapsed them to PM=48 and a "PM: x" echo removed whatever PM= entry was queued.

Run: python tests/test_pic_pm_chain.py
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

CORE_INO = Path(os.environ.get("OTGW_CORE_INO", REPO / "src" / "OTGW-firmware" / "OTGW-Core.ino"))

STUBS = r"""
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cctype>
#define PSTR(x) x
#define F(x) x
#define snprintf_P snprintf
#define OTDebugTln(...)
#define OTDebugTf(...)
#define OTDebugT(...)
#define OTDebugf(...)
#define OTDebug(...)
#define OTDebugFlush()
#define DebugTln(...)
#define CMDQUEUE_MAX 20
#define HAS_DIRECT_OT 1
struct OT_cmd_t { char cmd[15]; int cmdlen; int retrycnt; unsigned long due; };
static OT_cmd_t cmdqueue[CMDQUEUE_MAX];
static int cmdQueueSize = 0;
static uint32_t g_ms = 0;
static uint32_t millis() { return g_ms; }
static bool g_pic = true;
static bool isPICEnabled() { return g_pic; }
static bool isOTDirectEnabled() { return true; }
static char g_direct[256];
static void handleOTDirectCommand(const char* b, int n) { strncat(g_direct, b, n); strcat(g_direct, ";"); }
static bool satOwnsControlSetpoint() { return false; }
static bool satCommandInFlight() { return false; }
static void triggerPICsettingsReadout() {}
static char cMsg[128];
static void sendEventToWebSocket(char, const char*) {}
// Dropped-command feedback (TASK-1439) sits inside the cut-out range.
struct PicState { bool bCmdRetryExceeded = false; uint32_t iCmdDropped = 0; };
struct State { PicState pic; };
static State state;
#define CCONOFF(b) ((b) ? "ON" : "OFF")
static void sendMQTTData(const char*, const char*, bool) {}
static size_t strlcpy(char* d, const char* s, size_t n) { size_t l = strlen(s); if (n) { size_t c = l < n - 1 ? l : n - 1; memcpy(d, s, c); d[c] = 0; } return l; }
static char g_sent[512];
static void sendPICSerial(const char* b, int n) { strncat(g_sent, b, n); strcat(g_sent, ";"); }
#include <algorithm>
using std::min;
void addCommandToQueue(const char* buf, const int len, const bool forceQueue = false, const int16_t delay = 1000);
void pmChainStart();   // prototypes as in OTGW-Core.h
void pmChainOnBoilerResponse(uint8_t id);
void pmChainTick();
@BODY@
// one second of the main loop at time t, then print what went to the PIC
static void loopAt(uint32_t t) { g_ms = t; handleCommandQueue(); }
static void echo(const char* l) { checkCommandResponse(l, (unsigned)strlen(l)); }
static void show(const char* tag) { printf("%s=%s\n", tag, g_sent); g_sent[0] = 0; }
int main(int argc, char** argv) {
  const char* sc = argv[1];
  g_ms = 100000;
  if (!strcmp(sc, "init")) {             // initSAT, then a boiler that answers every PM= it is asked
#ifdef PM_CHAIN_TIMEOUT_MS
    pmChainStart();                      // fix: what initSAT calls now
#else
    addCommandToQueue("PM=3", 4, false, 0);    // HEAD: what initSAT queued
    addCommandToQueue("PM=15", 5, false, 0);
    addCommandToQueue("PM=48", 5, false, 0);
#endif
    char all[256] = "";
    for (uint32_t t = 100000; t <= 160000; t += 1000) {
      g_sent[0] = 0; loopAt(t);
      char line[64];
      const char* p = g_sent;
      while (sscanf(p, "%63[^;];", line) == 1) {
        strcat(all, line); strcat(all, ";");
        int id = atoi(line + 3); char e[16]; snprintf(e, sizeof(e), "PM: %d", id); echo(e);
#ifdef PM_CHAIN_TIMEOUT_MS
        pmChainOnBoilerResponse((uint8_t)id);
#endif
        p = strchr(p, ';') + 1;
      }
    }
    printf("asked=%s\nqueued=%d\n", all, cmdQueueSize);
    return 0;
  }
#ifdef PM_CHAIN_TIMEOUT_MS
  if (!strcmp(sc, "chain")) {            // all three answered, echo then response
    pmChainStart();
    loopAt(100000); show("t0");
    echo("PM: 15");                      // wrong value: must not remove the queued PM=3
    printf("queued_after_wrong_echo=%d\n", cmdQueueSize);
    echo("PM: 3"); printf("queued_after_echo=%d\n", cmdQueueSize);
    loopAt(101000); show("t1_no_response_yet");
    pmChainOnBoilerResponse(3);
    loopAt(101500); show("t2_after_id3");
    pmChainOnBoilerResponse(48);         // not the pending ID (15): ignored
    loopAt(102500); show("t2b_wrong_id");
    echo("PM: 15"); pmChainOnBoilerResponse(15);
    loopAt(103000); show("t3_after_id15");
    echo("PM: 48"); pmChainOnBoilerResponse(48);
    loopAt(104000); show("t4_done");
    printf("queued=%d\n", cmdQueueSize);
    return 0;
  }
  if (!strcmp(sc, "run")) {              // boiler answers the ids in argv[2] (bit0=3, bit1=15, bit2=48), 600 s of loop
    const unsigned mask = (unsigned)atoi(argv[2]);
    pmChainStart();
    int last = -1; char seq[256] = ""; int lastT = -1;
    for (int t = 0; t <= 600; t++) {
      g_sent[0] = 0; loopAt(100000 + t * 1000);
      const char* p = g_sent; char line[64];
      while (sscanf(p, "%63[^;];", line) == 1) {
        int id = atoi(line + 3); lastT = t;
        if (id != last) { char e[24]; snprintf(e, sizeof(e), "%d@%d,", id, t); strcat(seq, e); last = id; }
        int bit = id == 3 ? 1 : id == 15 ? 2 : 4;
        if (mask & bit) { char ec[16]; snprintf(ec, sizeof(ec), "PM: %d", id); echo(ec); pmChainOnBoilerResponse((uint8_t)id); }
        p = strchr(p, ';') + 1;
      }
    }
    printf("seq=%s\nlast_send=%d\n", seq, lastT);
    return 0;
  }
  if (!strcmp(sc, "response_before_echo")) {   // MsgID 3 response arrives before the PM echo
    pmChainStart();
    loopAt(100000); show("t0");
    pmChainOnBoilerResponse(3);
    loopAt(101000); show("t1");
    printf("queued=%d\n", cmdQueueSize);
    return 0;
  }
  if (!strcmp(sc, "user_pm")) {          // a PM= queued by a user (REST/MQTT) survives the chain steps
    pmChainStart();
    loopAt(100000); show("t0");
    addCommandToQueue("PM=7", 4, true, 600000);   // far in the future: stays queued, unsent
    pmChainOnBoilerResponse(3);
    loopAt(101000); show("t1");
    for (int i = 0; i < cmdQueueSize; i++) printf("q%d=%s\n", i, cmdqueue[i].cmd);
    return 0;
  }
  if (!strcmp(sc, "direct")) {           // OT-Direct board: no PIC, three direct PM=, chain not armed
    g_pic = false;
    pmChainStart();
    printf("direct=%s\n", g_direct);
    pmChainOnBoilerResponse(3);
    loopAt(200000); loopAt(300000);
    printf("sent=%s\nqueued=%d\ndirect_after=%s\n", g_sent, cmdQueueSize, g_direct);
    return 0;
  }
#endif
  return 2;
}
"""


def extract(src: str) -> str:
    a = src.index("#define OTGW_CMD_RETRY")
    b = src.index("//===================[ Send buffer to OTGW ]")
    return src[a:b]


class PmChain(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.dir = tempfile.TemporaryDirectory()
        d = Path(cls.dir.name)
        cpp = d / "t.cpp"
        cpp.write_text(STUBS.replace("@BODY@", extract(CORE_INO.read_text(encoding="utf-8"))), encoding="utf-8")
        cls.exe = compile_cpp(cpp, d / "t.exe")

    @classmethod
    def tearDownClass(cls):
        cls.dir.cleanup()

    def run_sc(self, name, *args):
        if not self.exe:
            self.skipTest("no host C++ compiler")
        r = subprocess.run([str(self.exe), name, *args], capture_output=True, text=True)
        if r.returncode == 2:
            self.skipTest(f"scenario {name} needs the PM chain, not in this copy (HEAD)")
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        return dict(line.split("=", 1) for line in r.stdout.splitlines())

    def test_sat_init_asks_all_three(self):
        out = self.run_sc("init")
        self.assertEqual(out["asked"], "PM=3;PM=15;PM=48;")
        self.assertEqual(out["queued"], "0")

    def test_chain_in_order_on_responses(self):
        out = self.run_sc("chain")
        self.assertEqual(out["t0"], "PM=3;")
        self.assertEqual(out["queued_after_wrong_echo"], "1")      # PM: 15 echo kept PM=3
        self.assertEqual(out["queued_after_echo"], "0")
        self.assertEqual(out["t1_no_response_yet"], "")           # PM=15 waits for the MsgID 3 response
        self.assertEqual(out["t2_after_id3"], "PM=15;")
        self.assertEqual(out["t2b_wrong_id"], "")
        self.assertEqual(out["t3_after_id15"], "PM=48;")
        self.assertEqual(out["t4_done"], "")
        self.assertEqual(out["queued"], "0")

    def test_silent_boiler_60s_timeout_then_one_reask(self):
        out = self.run_sc("run", "0")
        # three passes of 60 s, then each missed id once more, then stop
        self.assertEqual(out["seq"], "3@0,15@60,48@120,3@180,15@240,48@300,")
        self.assertLess(int(out["last_send"]), 330)

    def test_only_missed_ids_are_reasked(self):
        out = self.run_sc("run", "2")      # only MsgID 15 is answered
        self.assertEqual(out["seq"], "3@0,15@60,48@61,3@121,48@181,")

    def test_all_answered_no_reask(self):
        out = self.run_sc("run", "7")
        self.assertEqual(out["seq"], "3@0,15@1,48@2,")

    def test_response_before_echo(self):
        out = self.run_sc("response_before_echo")
        self.assertEqual(out["t1"], "PM=15;")
        self.assertEqual(out["queued"], "1")                      # PM=3 replaced by PM=15

    def test_user_pm_is_not_purged(self):
        out = self.run_sc("user_pm")
        self.assertEqual(out["t1"], "PM=15;")
        self.assertEqual(out["q0"], "PM=7")
        self.assertEqual(out["q1"], "PM=15")

    def test_not_armed_on_ot_direct(self):
        out = self.run_sc("direct")
        self.assertEqual(out["direct"], "PM=3;PM=15;PM=48;")
        self.assertEqual(out["sent"], "")
        self.assertEqual(out["queued"], "0")
        self.assertEqual(out["direct_after"], "PM=3;PM=15;PM=48;")   # nothing more after a response

    def test_hook_only_records(self):
        # processOT() holds the non-recursive OTStateLock; queueing from there deadlocks on OT-Direct.
        src = CORE_INO.read_text(encoding="utf-8")
        m = re.search(r"void pmChainOnBoilerResponse\(uint8_t id\) \{(.*?)^\}", src, re.S | re.M)
        if not m:
            self.skipTest("no PM chain in this copy")
        body = m.group(1)
        for forbidden in ("addCommandToQueue", "pmChainSend", "pmChainAdvance", "removeFromCmdQueue"):
            self.assertNotIn(forbidden, body)
        hook = re.search(r".*pmChainOnBoilerResponse\(OTdata\.id\);", src)
        self.assertIsNotNone(hook)
        self.assertIn("!OTdata.bLocalAnswer", hook.group(0))
        self.assertIn("!OTdata.bReplayed", hook.group(0))


if __name__ == "__main__":
    unittest.main(verbosity=2)
