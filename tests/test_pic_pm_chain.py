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

SAT_INO = Path(os.environ.get("OTGW_SAT_INO", REPO / "src" / "OTGW-firmware" / "SATcontrol.ino"))
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
#define PIC_CMD_MAX_LEN @PICMAX@   // read from the real OTGW-Core.h (TASK-1576)
struct OT_cmd_t { char cmd[PIC_CMD_MAX_LEN + 1]; int cmdlen; int retrycnt; unsigned long due; };
static OT_cmd_t cmdqueue[CMDQUEUE_MAX];
static int cmdQueueSize = 0;
static uint32_t g_ms = 0;
static uint32_t millis() { return g_ms; }
static bool g_pic = true;
static bool isPICEnabled() { return g_pic; }
static bool isOTDirectEnabled() { return true; }
static char g_direct[256];
// TASK-1665: the diagnostic recorder is never started here, so every capture point in the cut-out code is its one-read no-op.
struct DiagRecStub { bool active = false; void recOtFrame(char, uint32_t) {} void recPicCommand(const char*, size_t) {}
                     void recOtdCommand(const char*, size_t) {} void recResponse(const char*, size_t) {} };
static DiagRecStub diagRec;
static bool handleOTDirectCommand(const char* b, int n, bool = false) { strncat(g_direct, b, n); strcat(g_direct, ";"); return true; }
static bool g_satOwns = false;      // TASK-1575: SAT owns the control setpoint
static bool satOwnsControlSetpoint() { return g_satOwns; }
static bool g_inflight = false;     // the old global in-flight flag (read only by the code before TASK-1575)
static bool satCommandInFlight() { return g_inflight; }
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
static bool g_simBlock = false;   // TASK-1580: the bench simulation refuses PR= (satSimulationBlocksBusTx)
static bool sendPICSerial(const char* b, int n) {
  if (g_simBlock && b[0] == 'P' && b[1] == 'R') return false;   // held back: nothing reaches the PIC
  strncat(g_sent, b, n); strcat(g_sent, ";"); return true;
}
#include <algorithm>
using std::min;
// TASK-1569: result enum (OTGW-firmware.h) and the queue lock (OTGW-Core.ino globals, outside the cut-out range):
// the host lock is a real recursive timed mutex, so the threaded test below exercises real contention.
#include <mutex>
#include <thread>
#include <atomic>
enum class CmdQueueResult : uint8_t { Queued, Replaced, Sent, Dropped };
@QUEUE_TYPES@
static std::recursive_timed_mutex g_qmutex;
struct CmdQueueLock {
  bool locked = false;
  CmdQueueLock() { locked = g_qmutex.try_lock_for(std::chrono::milliseconds(100)); }
  ~CmdQueueLock() { if (locked) g_qmutex.unlock(); }
};
#ifdef OLD_VOID_ADD   // code before TASK-1569: addCommandToQueue returns nothing
void addCommandToQueue(const char* buf, const int len, const bool forceQueue = false, const int16_t delay = 1000);
#elif defined(HAS_SAT_OWN_PARAM)   // TASK-1575: the origin of a CS= travels with the call
CmdQueueResult addCommandToQueue(const char* buf, const int len, const bool forceQueue = false, const int16_t delay = 1000,
                                 const bool satOwn = false);
#else
CmdQueueResult addCommandToQueue(const char* buf, const int len, const bool forceQueue = false, const int16_t delay = 1000);
#endif
void pmChainStart();   // prototypes as in OTGW-Core.h
void pmChainOnBoilerResponse(uint8_t id);
void pmChainTick();
@BODY@
// one second of the main loop at time t, then print what went to the PIC
static void loopAt(uint32_t t) { g_ms = t; handleCommandQueue(); }
// TASK-1384 (entry 261): the real satBootPmChainWanted() cut out of SATcontrol.ino (empty on the code before the fix)
struct SatSettingsStub { bool bEnabled = false; };
struct SettingsStub { SatSettingsStub sat; };
static SettingsStub settings;
static bool hasOTCommandInterface() { return true; }
@BOOTPM@
static void echo(const char* l) { checkCommandResponse(l, (unsigned)strlen(l)); }
static void show(const char* tag) { printf("%s=%s\n", tag, g_sent); g_sent[0] = 0; }
int main(int argc, char** argv) {
  const char* sc = argv[1];
  g_ms = 100000;
  if (!strcmp(sc, "own")) {              // TASK-1575: SAT owns MsgID 1 and is enqueueing its own CS= on the loop task
    g_pic = true; g_satOwns = true; g_inflight = true;
    CmdQueueResult r = addCommandToQueue("CS=40.0", 7, false, 0);          // an external CS= (REST) at that moment
    printf("ext=%d\n", r == CmdQueueResult::Dropped ? 0 : 1);
#ifdef HAS_SAT_OWN_PARAM
    g_inflight = false;
    r = addCommandToQueue("CS=41.0", 7, false, 0, true);                    // SAT's own CS=
    printf("sat=%d\n", r == CmdQueueResult::Dropped ? 0 : 1);
#else
    printf("sat=1\n");
#endif
    return 0;
  }
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
  if (!strcmp(sc, "user_three")) {       // TASK-1315 (Robert): three PM= from REST/MQTT right after each other, no forceQueue
    addCommandToQueue("PM=3", 4);
    addCommandToQueue("PM=15", 5);
    addCommandToQueue("PM=48", 5);
    addCommandToQueue("PM=15", 5);        // an exact repeat of a queued PM= is not queued twice
    printf("queued_at_start=%d\n", cmdQueueSize);
    char all[256] = "", times[256] = "";
    for (uint32_t t = 101000; t <= 110000; t += 250) {    // 250 ms loop; the PIC echoes every PM= it receives
      g_sent[0] = 0; loopAt(t);
      char line[64]; const char* p = g_sent;
      while (sscanf(p, "%63[^;];", line) == 1) {
        strcat(all, line); strcat(all, ";");
        char ts[24]; snprintf(ts, sizeof(ts), "%lu;", (unsigned long)(t - 101000)); strcat(times, ts);
        char e[16]; snprintf(e, sizeof(e), "PM: %d", atoi(line + 3)); echo(e);
        pmChainOnBoilerResponse((uint8_t)atoi(line + 3));   // TASK-1572: the boiler answers the PM= (frees the slot)
        p = strchr(p, ';') + 1;
      }
    }
    printf("sent=%s\nsent_ms=%s\nqueued=%d\n", all, times, cmdQueueSize);
    return 0;
  }
  if (!strcmp(sc, "burst")) {            // TASK-1567: SAT's resend set due at once; the queue must drip, one per tick
    const unsigned step = (unsigned)atoi(argv[2]);   // loop period in ms (1000 = the firmware tick; 250 = a faster caller)
    addCommandToQueue("CS=45.0", 7, false, 0);
    addCommandToQueue("MM=80", 5, false, 0);
    addCommandToQueue("CH=1", 4, false, 0);
    char all[256] = "", times[256] = "";
    for (uint32_t t = 100000; t <= 106000; t += step) {
      g_sent[0] = 0; loopAt(t);
      char line[64]; const char* p = g_sent;
      while (sscanf(p, "%63[^;];", line) == 1) {
        strcat(all, line); strcat(all, ";");
        char ts[24]; snprintf(ts, sizeof(ts), "%lu;", (unsigned long)(t - 100000)); strcat(times, ts);
        char e[24]; snprintf(e, sizeof(e), "%c%c: %s", line[0], line[1], line + 3); echo(e);   // the PIC echoes it
        p = strchr(p, ';') + 1;
      }
    }
    printf("sent=%s\nsent_ms=%s\nqueued=%d\n", all, times, cmdQueueSize);
    return 0;
  }
  if (!strcmp(sc, "paced")) {            // TASK-1571: three commands, the PIC echoes each one at once; loop every 10 ms
    addCommandToQueue("CS=45.0", 7, false, 0);
    addCommandToQueue("MM=80", 5, false, 0);
    addCommandToQueue("CH=1", 4, false, 0);
    char all[128] = "", times[128] = "";
    for (uint32_t t = 100000; t <= 103000; t += 10) {
      g_sent[0] = 0; loopAt(t); strcat(all, g_sent);
      char line[32]; const char* p = g_sent;
      while (sscanf(p, "%31[^;];", line) == 1) {
        char ts[16]; snprintf(ts, sizeof(ts), "%lu;", (unsigned long)(t - 100000)); strcat(times, ts);
        char e[32]; snprintf(e, sizeof(e), "%c%c: %s", line[0], line[1], line + 3); echo(e);
        p = strchr(p, ';') + 1;
      }
    }
    printf("sent=%s\nsent_ms=%s\nqueued=%d\n", all, times, cmdQueueSize);
    return 0;
  }
  if (!strcmp(sc, "noecho")) {           // TASK-1571: no echo for the first command: the second waits for the timeout
    addCommandToQueue("CS=45.0", 7, false, 0);
    addCommandToQueue("MM=80", 5, false, 0);
    char times[128] = "", all[128] = "";
    for (uint32_t t = 100000; t <= 102500; t += 10) {
      g_sent[0] = 0; loopAt(t); strcat(all, g_sent);
      if (g_sent[0]) { char ts[16]; snprintf(ts, sizeof(ts), "%lu;", (unsigned long)(t - 100000)); strcat(times, ts); }
    }
    printf("sent=%s\nsent_ms=%s\n", all, times);
    return 0;
  }
  if (!strcmp(sc, "sim_block") || !strcmp(sc, "sim_off_silent")) {   // TASK-1580
    g_simBlock = !strcmp(sc, "sim_block");
    addCommandToQueue("PR=M", 4, false, 0);
    char all[256] = "";
    for (uint32_t t = 100000; t <= 160000; t += 100) {   // 60 s: well over 5 retry intervals and echo timeouts
      g_sent[0] = 0; loopAt(t); strcat(all, g_sent);
    }
    printf("sent=%s\nqueued=%d\ndropped=%u\nflag=%d\n", all, cmdQueueSize, (unsigned)state.pic.iCmdDropped, (int)state.pic.bCmdRetryExceeded);
    return 0;
  }
  if (!strcmp(sc, "sim_order")) {        // TASK-1580: a held-back PR=M must not delay the command behind it
    g_simBlock = true;                   // the stub refuses PR=... only here (see sendPICSerial)
    addCommandToQueue("PR=M", 4, false, 0);
    addCommandToQueue("CS=45.0", 7, false, 0);
    long csAt = -1;
    for (uint32_t t = 100000; t <= 110000 && csAt < 0; t += 10) { g_sent[0] = 0; loopAt(t); if (strstr(g_sent, "CS=45.0")) csAt = t - 100000; }
    printf("cs_at=%ld\nqueued=%d\ndropped=%u\n", csAt, cmdQueueSize, (unsigned)state.pic.iCmdDropped);
    return 0;
  }
  if (!strcmp(sc, "oe")) {               // TASK-1571 rule 4: OE (PIC busy) is resent, not dropped
    addCommandToQueue("CS=45.0", 7, false, 0);
    char all[128] = "";
    int n = 0;
    for (uint32_t t = 100000; t <= 101000; t += 10) {
      g_sent[0] = 0; loopAt(t); strcat(all, g_sent);
      if (g_sent[0]) { n++; echo(n == 1 ? "CS: OE" : "CS: 45.00"); }
    }
    printf("sent=%s\nqueued=%d\ndropped=%u\n", all, cmdQueueSize, (unsigned)state.pic.iCmdDropped);
    return 0;
  }
  if (!strcmp(sc, "deep_echo")) {        // TASK-1571: a CS= behind 19 commands that the PIC does echo
    char c[8];
    for (int k = 0; k < 19; k++) { snprintf(c, sizeof(c), "%c%c=1", 'A' + k / 6, 'A' + k % 6); addCommandToQueue(c, 4, false, 0); }
    addCommandToQueue("CS=45.0", 7, false, 0);
    long csAt = -1;
    for (uint32_t t = 100000; t <= 130000 && csAt < 0; t += 10) {
      g_sent[0] = 0; loopAt(t);
      char line[32]; const char* p = g_sent;
      while (sscanf(p, "%31[^;];", line) == 1) {
        if (!strcmp(line, "CS=45.0")) csAt = (long)(t - 100000);
        char e[32]; snprintf(e, sizeof(e), "%c%c: %s", line[0], line[1], line + 3); echo(e);
        p = strchr(p, ';') + 1;
      }
    }
    printf("cs_sent_ms=%ld\n", csAt);
    return 0;
  }
  if (!strcmp(sc, "stale_echo")) {       // review finding 5: CS=50 on the wire, replaced by CS=55, then the CS=50 echo
    addCommandToQueue("CS=50.0", 7, false, 0);
    g_sent[0] = 0; loopAt(100000);
    printf("first=%s\n", g_sent);
    addCommandToQueue("CS=55.0", 7, false, 0);   // replaces the queued entry while CS=50.0 is outstanding
    echo("CS: 50.00");
    printf("queued=%d\nq0=%s\n", cmdQueueSize, cmdQueueSize ? cmdqueue[0].cmd : "");
    return 0;
  }
  if (!strcmp(sc, "retry_back")) {       // review finding 3: an unconfirmed command retries behind newer commands
    addCommandToQueue("AA=1", 4, false, 0);       // never echoed
    g_sent[0] = 0; loopAt(100000);               // AA=1 sent, no echo
    addCommandToQueue("BB=1", 4, false, 0);
    addCommandToQueue("CC=1", 4, false, 0);
    char all[128] = "";
    for (uint32_t t = 101000; t <= 112000; t += 10) {
      g_sent[0] = 0; loopAt(t); strcat(all, g_sent);
      char line[32]; const char* p = g_sent;
      while (sscanf(p, "%31[^;];", line) == 1) {
        if (strcmp(line, "AA=1")) { char e[32]; snprintf(e, sizeof(e), "%c%c: %s", line[0], line[1], line + 3); echo(e); }
        p = strchr(p, ';') + 1;
      }
    }
    printf("sent=%s\n", all);
    return 0;
  }
  if (!strcmp(sc, "prio_slot")) {        // TASK-1572: PM=3 then PM=15; the PIC echoes, the boiler answers only at 20 s
    addCommandToQueue("PM=3", 4, false, 0);
    addCommandToQueue("PM=15", 5, false, 0);
    addCommandToQueue("CS=45.0", 7, false, 0);
    char times[256] = "";
    for (uint32_t t = 100000; t <= 125000; t += 10) {
      if (t == 120000) pmChainOnBoilerResponse(3);     // the boiler answers PM=3 at 20 s
      g_sent[0] = 0; loopAt(t);
      char line[32]; const char* p = g_sent;
      while (sscanf(p, "%31[^;];", line) == 1) {
        char ts[32]; snprintf(ts, sizeof(ts), "%s@%lu;", line, (unsigned long)(t - 100000)); strcat(times, ts);
        char e[32]; snprintf(e, sizeof(e), "%c%c: %s", line[0], line[1], line + 3); echo(e);
        p = strchr(p, ';') + 1;
      }
    }
    printf("sent=%s\nqueued=%d\n", times, cmdQueueSize);
    return 0;
  }
  if (!strcmp(sc, "prio_tp_pm")) {       // TASK-1572: TP=11:12=80 (Immergas) then a user PM=7; the boiler answers ID 11 at 5 s
    addCommandToQueue("TP=11:12=80", 11, false, 0);
    addCommandToQueue("PM=7", 4, false, 0);
    char times[256] = "";
    for (uint32_t t = 100000; t <= 110000; t += 10) {
      if (t == 105000) pmChainOnBoilerResponse(11);
      g_sent[0] = 0; loopAt(t);
      char line[32]; const char* p = g_sent;
      while (sscanf(p, "%31[^;];", line) == 1) {
        char ts[32]; snprintf(ts, sizeof(ts), "%s@%lu;", line, (unsigned long)(t - 100000)); strcat(times, ts);
        char e[32]; snprintf(e, sizeof(e), "%c%c: %s", line[0], line[1], line + 3); echo(e);
        p = strchr(p, ';') + 1;
      }
    }
    printf("sent=%s\n", times);
    return 0;
  }
  if (!strcmp(sc, "prio_timeout")) {     // TASK-1572: the boiler never answers PM=3; PM=15 goes out after 60 s
    addCommandToQueue("PM=3", 4, false, 0);
    addCommandToQueue("PM=15", 5, false, 0);
    long at15 = -1;
    for (uint32_t t = 100000; t <= 170000 && at15 < 0; t += 100) {
      g_sent[0] = 0; loopAt(t);
      char line[32]; const char* p = g_sent;
      while (sscanf(p, "%31[^;];", line) == 1) {
        if (!strcmp(line, "PM=15")) at15 = (long)(t - 100000);
        char e[32]; snprintf(e, sizeof(e), "%c%c: %s", line[0], line[1], line + 3); echo(e);
        p = strchr(p, ';') + 1;
      }
    }
    printf("pm15_ms=%ld\n", at15);
    return 0;
  }
  if (!strcmp(sc, "snap")) {             // TASK-1573: the queue snapshot behind /api/v2/device/info
    addCommandToQueue("PM=3", 4, false, 0);
    addCommandToQueue("CS=45.0", 7, false, 0);
    g_sent[0] = 0; loopAt(100000);             // PM=3 sent, waiting for its echo
    CmdQueueSnap a; cmdQueueSnapshot(&a);
    printf("a_valid=%d\na_depth=%u\na_inflight=%s\na_prio=%u\na_sent=%lu\n", a.valid ? 1 : 0, (unsigned)a.depth, a.inflight,
           (unsigned)a.prioSlot, (unsigned long)a.sent);
    g_ms = 100030; echo("PM: 3");               // echo after 30 ms
    g_sent[0] = 0; loopAt(100100);              // CS=45.0 goes, PM=3 still holds the slot (no boiler answer yet)
    g_ms = 100105; echo("CS: 45.00");           // echo after 5 ms
    addCommandToQueue("ZZ=1", 4, false, 0);
    g_ms = 100150; echo("ZZ: NG");              // not outstanding: falls back to the 2-letter path, refused
    CmdQueueSnap b; cmdQueueSnapshot(&b);
    printf("b_depth=%u\nb_inflight=%s\nb_prio=%u\nb_sent=%lu\nb_rejected=%lu\nb_echo=%lu\nb_min=%lu\nb_max=%lu\nb_h0=%lu\nb_h2=%lu\n",
           (unsigned)b.depth, b.inflight, (unsigned)b.prioSlot, (unsigned long)b.sent, (unsigned long)b.rejected,
           (unsigned long)b.echo.count, (unsigned long)b.echo.minMs, (unsigned long)b.echo.maxMs,
           (unsigned long)b.echo.hist[0], (unsigned long)b.echo.hist[2]);
    return 0;
  }
  if (!strcmp(sc, "maxlen")) {           // TASK-1576: 15 characters is the PIC maximum; 16 is refused, never cut
    const char* c15 = "XX=123456789012";      // 15
    const char* c16 = "YY=1234567890123";     // 16
#ifdef OLD_VOID_ADD
    addCommandToQueue(c15, 15, false, 0); addCommandToQueue(c16, 16, false, 0);
    printf("r15=%d\nr16=%d\n", -1, -1);
#else
    printf("r15=%d\nr16=%d\n", (int)addCommandToQueue(c15, 15, false, 0), (int)addCommandToQueue(c16, 16, false, 0));
#endif
    g_sent[0] = 0; loopAt(100000);
    printf("queued=%d\nsent=%s\n", cmdQueueSize, g_sent);
    return 0;
  }
  if (!strcmp(sc, "fifo")) {             // TASK-1569 rule 5b: an updated command moves to the back
    addCommandToQueue("MM=80", 5, false, 0);
    addCommandToQueue("CS=45.0", 7, false, 0);
    addCommandToQueue("MM=90", 5, false, 0);   // update of the queued MM=
    char all[128] = "";
    for (uint32_t t = 100000; t <= 104000; t += 1000) {
      g_sent[0] = 0; loopAt(t); strcat(all, g_sent);
      char line[32]; const char* p = g_sent;
      while (sscanf(p, "%31[^;];", line) == 1) { char e[32]; snprintf(e, sizeof(e), "%c%c: %s", line[0], line[1], line + 3); echo(e); p = strchr(p, ';') + 1; }
    }
    printf("sent=%s\n", all);
    return 0;
  }
  if (!strcmp(sc, "rate")) {             // TASK-1571 rule 1: a client repeats one prefix faster than the delay (1000 ms)
    // argv: mode ("tt" = TT=<changing value>, "pr" = PR=A exact repeat), period_ms, count. The PIC echoes after 25 ms.
    const char* mode = argv[2]; const int period = atoi(argv[3]); const int count = atoi(argv[4]);
    const uint32_t t0 = 100000, end = t0 + (uint32_t)period * count + 8000;
    int offered = 0, sends = 0; uint32_t echoAt = 0, lastSend = 0, maxGap = 0; char pend[32] = "";
    for (uint32_t t = t0; t <= end; t += 5) {
      g_ms = t;
      if (offered < count && t >= t0 + (uint32_t)period * offered) {
        char c[16];
        if (!strcmp(mode, "pr")) strcpy(c, "PR=A"); else snprintf(c, sizeof(c), "TT=%d.0", 10 + offered % 20);
        addCommandToQueue(c, (int)strlen(c), false, 1000);
        offered++;
      }
      if (echoAt && t >= echoAt) {
        char e[32];
        if (pend[0] == 'P') snprintf(e, sizeof(e), "PR: %c=1", pend[3]); else snprintf(e, sizeof(e), "%c%c: %s", pend[0], pend[1], pend + 3);
        echo(e); echoAt = 0;
      }
      g_sent[0] = 0; handleCommandQueue();
      if (g_sent[0]) {
        sends++; if (lastSend && t - lastSend > maxGap) maxGap = t - lastSend; lastSend = t;
        strncpy(pend, g_sent, sizeof(pend) - 1); char* s = strchr(pend, ';'); if (s) *s = 0;
        echoAt = t + 25;
      }
    }
    printf("offered=%d\nsends=%d\nmax_gap_ms=%lu\nleft=%d\n", offered, sends, (unsigned long)maxGap, cmdQueueSize);
    return 0;
  }
  if (!strcmp(sc, "pr_ab")) {            // TASK-1571 rule 2: PR=A then PR=B within the delay; each is confirmed by its own echo
    addCommandToQueue("PR=A", 4, false, 1000);
    addCommandToQueue("PR=B", 4, false, 1000);
    char all[64] = "";
    for (uint32_t t = 100000; t <= 106000; t += 5) {
      g_sent[0] = 0; loopAt(t); strcat(all, g_sent);
      char line[32]; const char* p = g_sent;
      while (sscanf(p, "%31[^;];", line) == 1) { char e[32]; snprintf(e, sizeof(e), "PR: %c=1", line[3]); echo(e); p = strchr(p, ';') + 1; }
    }
    printf("sent=%s\nqueued=%d\n", all, cmdQueueSize);
    return 0;
  }
  if (!strcmp(sc, "pr_repeat")) {        // TASK-1571 rule 2: an exact repeat of PR=A still coalesces
    addCommandToQueue("PR=A", 4, false, 1000);
    addCommandToQueue("PR=A", 4, false, 1000);
    const int queued = cmdQueueSize;
    char all[64] = "";
    for (uint32_t t = 100000; t <= 103000; t += 5) {
      g_sent[0] = 0; loopAt(t); strcat(all, g_sent);
      if (g_sent[0]) echo("PR: A=1");
    }
    printf("queued=%d\nsent=%s\n", queued, all);
    return 0;
  }
  if (!strcmp(sc, "due_wrap")) {         // TASK-1571 rule 1 across the millis() wrap: the earliest due is kept
    const uint32_t start = 0xFFFFFE00u;   // 512 ms before the wrap
    g_ms = start; addCommandToQueue("TT=20.0", 7, false, 1000);        // due start+1000 (after the wrap)
    g_ms = start + 300; addCommandToQueue("TT=21.0", 7, false, 1000);   // newer text, later due: keeps start+1000
    long at = -1; char got[32] = "";
    for (uint32_t d = 0; d <= 3000 && at < 0; d += 5) { g_sent[0] = 0; loopAt(start + d); if (g_sent[0]) { at = (long)d; strcpy(got, g_sent); } }
    printf("sent=%s\nsent_at_ms=%ld\n", got, at);
    return 0;
  }
  if (!strcmp(sc, "tp_mm")) {            // Q-1537b: TP=11:12 then MM= offered in that order, both already queued
    addCommandToQueue("MM=80", 5, false, 0);
    addCommandToQueue("TP=11:12=80", 11, false, 0);
    addCommandToQueue("TP=11:12=70", 11, false, 0);   // SAT's next tick: new TP first ...
    addCommandToQueue("MM=70", 5, false, 0);          // ... then the new MM
    char all[160] = "";
    for (uint32_t t = 100000; t <= 106000; t += 1000) {
      g_sent[0] = 0; loopAt(t); strcat(all, g_sent);
      char line[32]; const char* p = g_sent;
      while (sscanf(p, "%31[^;];", line) == 1) { char e[32]; snprintf(e, sizeof(e), "%c%c: %s", line[0], line[1], line + 3); echo(e); if (line[0] == 'T') pmChainOnBoilerResponse(11); p = strchr(p, ';') + 1; }
    }
    printf("sent=%s\n", all);
    return 0;
  }
#ifndef OLD_VOID_ADD
  if (!strcmp(sc, "result")) {           // TASK-1569: addCommandToQueue reports what it did
    printf("new=%d\n", (int)addCommandToQueue("MM=80", 5));
    printf("again=%d\n", (int)addCommandToQueue("MM=90", 5));
    printf("invalid=%d\n", (int)addCommandToQueue("XX", 2));
    char c[8];
    for (int k = 0; k < 19; k++) { snprintf(c, sizeof(c), "%c%c=1", 'A' + k / 6, 'A' + k % 6); addCommandToQueue(c, 4); }
    printf("full_read=%d\n", (int)addCommandToQueue("PR=Q", 4, true));   // read into a full queue of writes: dropped
    g_pic = false;
    printf("direct=%d\n", (int)addCommandToQueue("MM=50", 5));
    return 0;
  }
#endif
  if (!strcmp(sc, "threads")) {          // TASK-1569: a second task adds while the loop drains and echoes
    std::atomic<bool> stop(false);
    std::atomic<long> added(0);
    std::thread producer([&]() {
      char c[8];
      for (int k = 0; k < 4000; k++) {
        snprintf(c, sizeof(c), "%c%c=%d", 'A' + (k % 7), 'A' + (k / 7) % 5, k % 10);
#ifdef OLD_VOID_ADD
        addCommandToQueue(c, (int)strlen(c), false, 0); added++;
#else
        if (addCommandToQueue(c, (int)strlen(c), false, 0) != CmdQueueResult::Dropped) added++;
#endif
      }
      stop = true;
    });
    long bad = 0, maxSize = 0, loops = 0;
    while (!stop || cmdQueueSize > 0) {
      { CmdQueueLock l; g_ms += 1000; g_sent[0] = 0; }
      handleCommandQueue();
      char line[32]; const char* p = g_sent;
      while (sscanf(p, "%31[^;];", line) == 1) {
        if (strlen(line) < 4 || line[2] != '=') bad++;
        char e[32]; snprintf(e, sizeof(e), "%c%c: %s", line[0], line[1], line + 3); echo(e);
        p = strchr(p, ';') + 1;
      }
      { CmdQueueLock l;
        if (cmdQueueSize < 0 || cmdQueueSize > CMDQUEUE_MAX) bad++;
        if (cmdQueueSize > maxSize) maxSize = cmdQueueSize;
        for (int i = 0; i < cmdQueueSize; i++) if (cmdqueue[i].cmdlen != (int)strlen(cmdqueue[i].cmd) || cmdqueue[i].cmd[2] != '=') bad++; }
      if (++loops > 200000) break;
    }
    producer.join();
    printf("bad=%ld\nmax_size=%ld\nadded=%ld\nend_size=%d\n", bad, maxSize, (long)added, cmdQueueSize);
    return 0;
  }
  if (!strcmp(sc, "deep")) {             // TASK-1567 review: a CS= behind a full queue (20 slots), the PIC echoes nothing
    char c[8];
    for (int k = 0; k < 19; k++) { snprintf(c, sizeof(c), "%c%c=1", 'A' + k / 6, 'A' + k % 6); addCommandToQueue(c, 4, false, 0); }
    addCommandToQueue("CS=45.0", 7, false, 0);
    printf("queued_at_start=%d\n", cmdQueueSize);
    long csAt = -1;
    for (uint32_t t = 100000; t <= 160000 && csAt < 0; t += 1000) {
      g_sent[0] = 0; loopAt(t);
      if (strstr(g_sent, "CS=45.0")) csAt = (long)(t - 100000);
    }
    printf("cs_sent_ms=%ld\n", csAt);
    return 0;
  }
  if (!strcmp(sc, "user_tp")) {          // TP= follows the same rule: unique per value, exact repeat not added twice
    addCommandToQueue("TP=11:12=40", 11);
    addCommandToQueue("TP=11:12=45", 11);
    addCommandToQueue("TP=11:12=40", 11);   // exact repeat
    addCommandToQueue("TP=11:13", 8);
    printf("queued_at_start=%d\n", cmdQueueSize);
    for (int i = 0; i < cmdQueueSize; i++) printf("q%d=%s\n", i, cmdqueue[i].cmd);
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
  if (!strcmp(sc, "boot")) {             // TASK-1384: what initSAT() does at boot. argv: pic(0/1) satEnabled(0/1)
    g_pic = atoi(argv[2]) != 0; settings.sat.bEnabled = atoi(argv[3]) != 0;
#ifdef HAS_BOOT_PM_GATE
    if (satBootPmChainWanted()) pmChainStart();
#else
    if (settings.sat.bEnabled) pmChainStart();   // code before the fix: only with SAT enabled
#endif
    loopAt(100000);
    printf("sent=%s\nqueued=%d\ndirect=%s\n", g_sent, cmdQueueSize, g_direct);
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
        body = extract(CORE_INO.read_text(encoding="utf-8"))
        prefix = "#define OLD_VOID_ADD\n" if "\nvoid addCommandToQueue(" in body else ""
        if "const bool satOwn" in body:
            prefix += "#define HAS_SAT_OWN_PARAM\n"   # TASK-1575
        hdr = (REPO / "src" / "OTGW-firmware" / "OTGW-firmware.h").read_text(encoding="utf-8")
        a = hdr.index("struct CmdQueueEchoStats")
        b = hdr.index("void cmdQueueSnapshot(CmdQueueSnap* out);")
        types = hdr[a:b].replace("extern CmdQueueEchoStats g_cmdQueueEcho;", "")   # the types from the real header
        core_h = (REPO / "src" / "OTGW-firmware" / "OTGW-Core.h").read_text(encoding="utf-8")
        m = re.search(r"^#define\s+PIC_CMD_MAX_LEN\s+(\d+)", core_h, re.M)
        picmax = m.group(1) if m else "14"   # before TASK-1576: char cmd[15]
        sat_src = SAT_INO.read_text(encoding="utf-8").replace("\r\n", "\n")
        mg = re.search(r"^static bool satBootPmChainWanted\(\)\n\{.*?^\}\n", sat_src, re.S | re.M)
        if mg:
            prefix += "#define HAS_BOOT_PM_GATE\n"
        boot_pm = mg.group(0) if mg else ""
        cpp.write_text(prefix + STUBS.replace("@BOOTPM@", boot_pm).replace("@QUEUE_TYPES@", types).replace("@PICMAX@", picmax).replace("@BODY@", body), encoding="utf-8")
        cls.exe = compile_cpp(cpp, d / "t.exe")

    @classmethod
    def tearDownClass(cls):
        cls.dir.cleanup()

    def run_sc(self, name, *args):
        if not self.exe:
            self.skipTest("no host C++ compiler")
        r = subprocess.run([str(self.exe), name, *args], capture_output=True, text=True)
        if r.returncode == 2:
            self.skipTest(f"scenario {name} not in this copy (older code)")
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        return dict(line.split("=", 1) for line in r.stdout.splitlines())

    def test_sat_init_asks_all_three(self):
        out = self.run_sc("init")
        self.assertEqual(out["asked"], "PM=3;PM=15;PM=48;")
        self.assertEqual(out["queued"], "0")

    def test_user_pm_three_in_a_row_all_reach_the_pic(self):
        # Robert 2026-11-01: three PM= that arrive right after each other (REST/MQTT, no forceQueue) must all go
        # to the PIC, not only the last. HEAD: the 2-letter prefix dedup keeps one PM= slot, so only PM=48 is sent.
        out = self.run_sc("user_three")
        self.assertEqual(out["queued_at_start"], "3")
        # TASK-1569 rule 5b: the exact repeat of PM=15 (offered last) moves to the back, so PM=48 goes before it
        self.assertEqual(out["sent"], "PM=3;PM=48;PM=15;")
        self.assertEqual(out["queued"], "0")
        # one second apart (Robert): the PIC keeps one pending priority message
        ms = [int(x) for x in out["sent_ms"].strip(";").split(";")]
        self.assertTrue(all(b - a >= 1000 for a, b in zip(ms, ms[1:])), f"PM= sent less than 1 s apart: {ms}")

    def test_queue_drips_one_command_per_tick(self):
        # TASK-1567 (Robert 2026-11-02): the queue never bursts. Three commands due at once go to the PIC on three
        # successive ticks, also when the queue is polled faster than the 1 s firmware tick. HEAD: all three in tick 0.
        for step in ("1000", "250"):
            with self.subTest(loop_ms=step):
                out = self.run_sc("burst", step)
                self.assertEqual(out["sent"], "CS=45.0;MM=80;CH=1;")
                ms = [int(x) for x in out["sent_ms"].strip(";").split(";")]
                # TASK-1571 replaced the 1 s tick by echo pacing; the floor between two sends is 50 ms
                self.assertTrue(all(b - a >= 50 for a, b in zip(ms, ms[1:])), f"sent within the 50 ms floor: {ms}")
                self.assertLessEqual(ms[-1], 5000, f"SAT resend set not drained within its 5 s period: {ms}")
                self.assertEqual(out["queued"], "0")

    def test_echo_paced_drain(self):
        # TASK-1571 (Robert 2026-11-02): the next command follows the echo of the outstanding one plus a 50 ms guard,
        # not a fixed 1 s tick. HEAD (TASK-1567 tick): the third command leaves 2000 ms after the first.
        out = self.run_sc("paced")
        self.assertEqual(out["sent"], "CS=45.0;MM=80;CH=1;")
        ms = [int(x) for x in out["sent_ms"].strip(";").split(";")]
        self.assertTrue(all(b - a >= 50 for a, b in zip(ms, ms[1:])), f"two sends within the 50 ms floor: {ms}")
        self.assertLess(ms[-1] - ms[0], 500, f"not paced by the echo: {ms}")
        self.assertEqual(out["queued"], "0")

    def test_no_echo_waits_for_the_timeout(self):
        # TASK-1571: one command outstanding; without an echo the next one waits 1000 ms.
        out = self.run_sc("noecho")
        ms = [int(x) for x in out["sent_ms"].strip(";").split(";")]
        self.assertGreaterEqual(len(ms), 2, out)
        self.assertGreaterEqual(ms[1] - ms[0], 1000, f"second command sent while the first was outstanding: {ms}")

    def test_overrun_is_resent_not_dropped(self):
        # TASK-1571 rule 4: OE = the PIC was busy; the command is sent again and confirmed, not reported as refused.
        out = self.run_sc("oe")
        self.assertEqual(out["sent"], "CS=45.0;CS=45.0;")
        self.assertEqual(out["queued"], "0")
        self.assertEqual(out["dropped"], "0")

    def test_simulation_held_back_command_is_consumed_not_dropped(self):
        # TASK-1580: sendPICSerial() refuses while the bench simulation is on. The queue must consume the entry,
        # not retry it five times and report PR=M as dropped every minute (command_retry_exceeded latched ON).
        out = self.run_sc("sim_block")
        self.assertEqual(out["dropped"], "0", out)
        self.assertEqual(out["flag"], "0", out)
        self.assertEqual(out["queued"], "0", out)

    def test_simulation_off_silent_pic_still_drops_after_five_tries(self):
        # Same command, simulation off, PIC never echoes: unchanged behaviour, dropped once after 5 tries.
        out = self.run_sc("sim_off_silent")
        self.assertEqual(out["sent"], "PR=M;" * 5, out)
        self.assertEqual(out["dropped"], "1", out)
        self.assertEqual(out["flag"], "1", out)
        self.assertEqual(out["queued"], "0", out)

    def test_simulation_held_back_command_does_not_delay_the_next(self):
        out = self.run_sc("sim_order")
        self.assertEqual(out["dropped"], "0", out)
        self.assertTrue(0 <= int(out["cs_at"]) < 1000, out)

    def test_external_cs_refused_while_sat_enqueues_its_own(self):
        # TASK-1575: with the old global in-flight flag set by SAT on the loop task, a CS= from REST at the same moment
        # passed as SAT's own. The origin is now an argument: the external one is refused, SAT's own still goes in.
        out = self.run_sc("own")
        self.assertEqual((out["ext"], out["sat"]), ("0", "1"))

    def test_cs_behind_a_full_echoing_queue_within_two_seconds(self):
        # TASK-1571 AC3: with echoes the 20-deep queue drains in about a second; the CS= at the back goes out < 2 s.
        out = self.run_sc("deep_echo")
        cs = int(out["cs_sent_ms"])
        self.assertGreaterEqual(cs, 0, "CS= never sent")
        self.assertLess(cs, 2000, f"CS= behind 19 echoed commands waited {cs} ms")
        print(f"\n  CS= behind 19 echoed commands went out after {cs} ms")

    def test_stale_echo_does_not_remove_the_replacement(self):
        # TASK-1569 review finding 5: the echo of CS=50.0 arrives after CS=55.0 replaced it; CS=55.0 must stay queued.
        out = self.run_sc("stale_echo")
        self.assertEqual(out["first"], "CS=50.0;")
        self.assertEqual(out["queued"], "1")
        self.assertEqual(out["q0"], "CS=55.0")

    def test_unconfirmed_command_retries_behind_newer_ones(self):
        # TASK-1569 review finding 3: AA=1 is never echoed; BB=1 and CC=1 offered after it go first, AA=1 retries after.
        out = self.run_sc("retry_back")
        sent = out["sent"].strip(";").split(";")
        self.assertEqual(sent[:2], ["BB=1", "CC=1"], out["sent"])
        self.assertIn("AA=1", sent[2:])

    @staticmethod
    def _times(sent):
        out = {}
        for item in sent.strip(";").split(";"):
            cmd, ms = item.rsplit("@", 1)
            out.setdefault(cmd, []).append(int(ms))
        return out

    def test_priority_slot_waits_for_the_boiler(self):
        # TASK-1572 (rule 3): PM=15 is not sent while PM=3 waits for its boiler answer, even after the PIC's echo;
        # CS=45.0 keeps draining meanwhile. HEAD (TASK-1571): PM=15 one second after PM=3.
        out = self.run_sc("prio_slot")
        t = self._times(out["sent"])
        self.assertIn("CS=45.0", t)
        self.assertLess(t["CS=45.0"][0], 2000, "CS= held back by the priority slot")
        self.assertGreaterEqual(t["PM=15"][0], 20000, f"PM=15 sent before the boiler answered PM=3: {out['sent']}")
        self.assertLess(t["PM=15"][0], 21500, f"PM=15 late after the answer: {out['sent']}")

    def test_tp_and_pm_do_not_cancel_each_other(self):
        # TASK-1572: a user PM=7 waits until the boiler answered the Immergas TP=11:12 (DataID 11).
        out = self.run_sc("prio_tp_pm")
        t = self._times(out["sent"])
        self.assertEqual(t["TP=11:12=80"][0], 0)
        self.assertGreaterEqual(t["PM=7"][0], 5000, out["sent"])

    def test_priority_slot_frees_after_60_s(self):
        # TASK-1572: a boiler that never answers holds the slot at most 60 s.
        out = self.run_sc("prio_timeout")
        ms = int(out["pm15_ms"])
        self.assertGreaterEqual(ms, 60000, f"PM=15 sent at {ms} ms")
        self.assertLess(ms, 62000, f"PM=15 sent at {ms} ms")

    def test_queue_snapshot_for_device_info(self):
        # TASK-1573 (rule 6): depth, the command in flight, the priority slot, counters and the echo histogram.
        out = self.run_sc("snap")
        self.assertEqual((out["a_valid"], out["a_depth"], out["a_inflight"], out["a_prio"], out["a_sent"]),
                         ("1", "2", "PM=3", "3", "1"))
        self.assertEqual(out["b_inflight"], "")
        self.assertEqual(out["b_prio"], "3")          # still waiting for the boiler
        self.assertEqual(out["b_sent"], "2")
        self.assertEqual(out["b_rejected"], "1")
        self.assertEqual((out["b_echo"], out["b_min"], out["b_max"]), ("2", "5", "30"))
        self.assertEqual((out["b_h0"], out["b_h2"]), ("1", "1"))   # 5 ms -> <10 bucket, 30 ms -> <50 bucket

    def test_command_length_matches_the_pic_buffer(self):
        # TASK-1576: the PIC rxbuffer holds 15 characters (gateway.asm rxbuffer res 16, SerialReceive). HEAD: the queue
        # held 14, so the 15-character command was refused; a 16-character one must be refused, not cut by the PIC.
        out = self.run_sc("maxlen")
        self.assertEqual(out["sent"], "XX=123456789012;")
        self.assertEqual(out["queued"], "1")
        self.assertEqual(out["r16"], "3")   # Dropped

    def test_updated_command_moves_to_the_back(self):
        # TASK-1569 rule 5b (Robert 2026-11-02). HEAD: MM=90 kept MM=80's first slot and went out before CS=45.0.
        out = self.run_sc("fifo")
        self.assertEqual(out["sent"], "CS=45.0;MM=90;")

    def test_repeating_client_is_not_starved(self):
        # TASK-1571 rule 1 (Robert 2026-11-04): the same prefix every 200 ms for 24 s (delay 1000 ms) must still
        # reach the PIC about once per second. HEAD: every repeat re-armed due = now + 1000, so only 1 send in total.
        for mode in ("tt", "pr"):
            with self.subTest(mode=mode):
                out = self.run_sc("rate", mode, "200", "120")
                self.assertGreaterEqual(int(out["sends"]), 18, out)
                self.assertLessEqual(int(out["max_gap_ms"]), 1300, out)
                print(f"\n  {mode}: 120 offers / 24 s -> {out['sends']} sends, max gap {out['max_gap_ms']} ms")

    def test_earliest_due_survives_the_millis_wrap(self):
        out = self.run_sc("due_wrap")
        self.assertEqual(out["sent"], "TT=21.0;")   # newest text wins
        self.assertLessEqual(abs(int(out["sent_at_ms"]) - 1000), 10, out)   # not 1300 (re-armed)

    def test_pr_different_registers_both_go_out(self):
        # TASK-1571 rule 2: only an exact repeat of PR= coalesces. HEAD: PR=B replaced PR=A, only PR=B was sent.
        out = self.run_sc("pr_ab")
        self.assertEqual(out["sent"], "PR=A;PR=B;")
        self.assertEqual(out["queued"], "0")

    def test_pr_exact_repeat_still_coalesces(self):
        out = self.run_sc("pr_repeat")
        self.assertEqual(out["queued"], "1")
        self.assertEqual(out["sent"], "PR=A;")

    def test_tp_offered_before_mm_goes_first(self):
        # Q-1537b / entry 233 (Immergas): TP=11:12 is offered right before MM=, so it must reach the PIC first.
        # HEAD: MM= kept its older slot and went first.
        out = self.run_sc("tp_mm")
        sent = out["sent"].strip(";").split(";")
        self.assertIn("TP=11:12=70", sent)
        self.assertIn("MM=70", sent)
        self.assertLess(sent.index("TP=11:12=70"), sent.index("MM=70"), out["sent"])
        self.assertNotIn("MM=80", sent)

    def test_add_reports_its_result(self):
        # TASK-1569: Queued=0, Replaced=1, Sent=2, Dropped=3 (CmdQueueResult order)
        out = self.run_sc("result")
        self.assertEqual((out["new"], out["again"], out["invalid"], out["full_read"], out["direct"]),
                         ("0", "1", "3", "3", "2"))

    def test_concurrent_add_and_drain_keep_the_queue_consistent(self):
        # TASK-1569: a producer thread adds 4000 commands while the loop drains and echoes, with a real recursive
        # timed mutex on the host. The queue never holds a torn entry, never exceeds 20, and empties at the end.
        out = self.run_sc("threads")
        self.assertEqual(out["bad"], "0", out)
        self.assertLessEqual(int(out["max_size"]), 20)
        self.assertEqual(out["end_size"], "0")
        self.assertGreater(int(out["added"]), 0)

    def test_cs_behind_a_full_queue_still_well_within_the_pic_minute(self):
        # TASK-1567 review (lead): worst case for a CS= at the back of a full queue (20 slots), no PIC echoes, so every
        # earlier command keeps retrying every 5 s. The PIC drops a CS= after about a minute without a new one
        # (otgw.tclcode.com/firmware.html: 'must be repeated at least every minute'); the drip must stay well inside that.
        out = self.run_sc("deep")
        self.assertEqual(out["queued_at_start"], "20")
        cs = int(out["cs_sent_ms"])
        self.assertGreaterEqual(cs, 0, "CS= never sent")
        self.assertLessEqual(cs, 30000, f"CS= waited {cs} ms behind a full queue")
        print(f"\n  CS= behind 19 unconfirmed commands went out after {cs} ms")

    def test_user_tp_unique_per_value(self):
        # Robert 2026-11-01: TP= the same way as PM=. Different values each get an entry; an exact repeat does not.
        # Before: TP= was append-only (TASK-1439), so the repeat of TP=11:12=40 was queued a second time.
        out = self.run_sc("user_tp")
        self.assertEqual(out["queued_at_start"], "3")
        # TASK-1569 rule 5b: the exact repeat of TP=11:12=40 (offered third) moves behind TP=11:12=45
        self.assertEqual([out["q0"], out["q1"], out["q2"]], ["TP=11:12=45", "TP=11:12=40", "TP=11:13"])

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

    # TASK-1384 (entry 261): the boot PM chain runs on a PIC board whatever SAT is; OT-Direct stays as it was (SAT enabled only).
    # Red on the code before the fix: PIC with SAT disabled sends nothing.
    def test_boot_chain_on_pic_with_sat_disabled(self):
        out = self.run_sc("boot", "1", "0")
        self.assertEqual(out["sent"], "PM=3;", out)

    def test_boot_chain_on_pic_with_sat_enabled_unchanged(self):
        out = self.run_sc("boot", "1", "1")
        self.assertEqual(out["sent"], "PM=3;", out)

    def test_boot_chain_ot_direct_unchanged(self):
        off = self.run_sc("boot", "0", "0")
        self.assertEqual((off["direct"], off["sent"]), ("", ""), off)          # SAT disabled: nothing (the board polls itself)
        on = self.run_sc("boot", "0", "1")
        self.assertEqual(on["direct"], "PM=3;PM=15;PM=48;", on)                # SAT enabled: three direct PM=

    def test_init_sat_starts_the_chain_outside_the_sat_enabled_block(self):
        src = SAT_INO.read_text(encoding="utf-8").replace("\r\n", "\n")
        body = re.search(r"^void initSAT\(\)\n\{\n(.*?)^\}\n", src, re.S | re.M).group(1)
        self.assertIn("if (satBootPmChainWanted()) pmChainStart();", body)
        enabled = re.search(r"  if \(settings\.sat\.bEnabled\) \{\n(.*?)\n  \} else \{", body, re.S)
        self.assertIsNotNone(enabled)
        self.assertNotIn("pmChainStart", enabled.group(1), "the chain must not sit inside the SAT-enabled block")

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
