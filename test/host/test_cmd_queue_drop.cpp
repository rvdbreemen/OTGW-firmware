//=======================================================================
// test_cmd_queue_drop.cpp
//
// TASK-1439 host harness, driven by test_cmd_queue_drop.py. Compiles the REAL
// command queue from OTGW-Core.ino (from "#define OTGW_CMD_RETRY" through
// checkCommandResponse(): removeFromCmdQueue, addCommandToQueue,
// handleCommandQueue, checkCommandResponse and, on the fix, the dropped-command
// reporting) and the REAL PICSection from PICtypes.h, sliced by anchor from
// the revision under test. Only I/O is doubled: PIC serial, MQTT (captured),
// WebSocket, debug, millis(), and the SAT/PIC-presence guards.
//
// Contract (same as 1.x TASK-1431): a command dropped after 5 unanswered
// tries is published on otgw-firmware/command_dropped (not retained) and
// latches otgw-firmware/command_retry_exceeded ON (retained) until the next
// confirmed command that is not PR= or PM=. TP= is appended, never replaced.
//
// Cases (CASE <id> pass|FAIL):
//   D1  PR=Z unanswered 5 times -> command_dropped "PR=Z" (retain 0),
//       command_retry_exceeded "ON" (retain 1), cmd_dropped 1 (when the field exists)
//   D2  after D1 set the flag ON, a PR= and a PM= confirmation do not clear it
//   D3  a confirmed TT= clears it: command_retry_exceeded "OFF" (retain 1)
//   D4  two TP= queued back to back (no forceQueue) occupy two slots
//   C1  two TT= queued back to back (no forceQueue) share one slot, last wins
//   C2  a command answered before its 5th try is not reported
//   R1  ZZ=1 answered "ZZ: NG" -> command_dropped "ZZ=1 (NG)", flag ON, one send (TASK-1457)
//   R2  with the flag ON, SB=99 answered "SB: OR" -> reported, flag not OFF
//   R3  PR=Q answered "PR: BV" -> "PR=Q (BV)" after one send
//   R4  PM=3 answered "PM: NG" -> "PM=3 (NG)" after one send
//=======================================================================

#include <cstdio>
#include <cstring>
#include <cstdint>
#include <cctype>
#include <string>
#include <vector>
#include <algorithm>
#include <type_traits>

// ---- platform doubles -------------------------------------------------
class __FlashStringHelper;
#define F(s) (reinterpret_cast<const __FlashStringHelper*>(s))
#define PSTR(s) (s)
#define CCONOFF(x) (x ? "ON" : "OFF")
#define HAS_DIRECT_OT 0
using std::min;
static inline size_t strlcpy(char* d, const char* s, size_t n) {
  size_t l = std::strlen(s);
  if (n) { size_t c = l < n - 1 ? l : n - 1; std::memcpy(d, s, c); d[c] = '\0'; }
  return l;
}
#define snprintf_P snprintf

static uint32_t g_millis = 100000;
static uint32_t millis() { return g_millis; }

// Debug output is compiled out. The firmware's OTDebug* macros (GCC statement
// expressions in OTGW-Core.ino) only print, so no-ops change no logic.
#define OTDebugTln(...) ((void)0)
#define OTDebugln(...)  ((void)0)
#define OTDebugTf(...)  ((void)0)
#define OTDebugf(...)   ((void)0)
#define OTDebugT(...)   ((void)0)
#define OTDebug(...)    ((void)0)
#define OTDebugFlush()  ((void)0)
#define DebugTln(...)   ((void)0)

struct Pub { std::string topic, payload; bool retain; };
static std::vector<Pub> g_pubs;
bool sendMQTTData(const __FlashStringHelper* topic, const char* value, const bool retain = false) {
  g_pubs.push_back({reinterpret_cast<const char*>(topic), value, retain});
  return true;
}
static std::vector<std::string> g_serial;
static bool sendPICSerial(const char* cmd, int len) { g_serial.emplace_back(cmd, (size_t)len); return true; }   // true: on the bus (TASK-1580)
static void sendEventToWebSocket(char, const char*) {}
static void pmChainTick() {}
static void triggerPICsettingsReadout() {}
static bool isPICEnabled() { return true; }
static bool satOwnsControlSetpoint() { return false; }
static bool satCommandInFlight() { return false; }
static char cMsg[128];

// ---- sliced firmware (revision under test) -----------------------------
#define PIC_UPDATE_IDLE 0
#include "cmdq_pic_section.inc"
struct DebugDouble { bool bOTmsg = false; };
struct StateDouble { PICSection pic; DebugDouble debug; };
static StateDouble state;

// The command-queue lock is the firmware's own CmdQueueLock; only the FreeRTOS mutex under it is doubled
// (one task, never contended). CmdQueueEchoStats, CmdQueueResult, OT_cmd_t and CMDQUEUE_MAX are the
// firmware's declarations too (cmdq_support.inc).
using PlatformMutex = void*;
static PlatformMutex cmdQueueMutex = nullptr;
static bool platformRecursiveMutexLock(PlatformMutex, uint32_t = 0) { return true; }
static void platformRecursiveMutexUnlock(PlatformMutex) {}
#include "cmdq_support.inc"
extern CmdQueueEchoStats g_cmdQueueEcho;
struct OT_cmd_t cmdqueue[CMDQUEUE_MAX];
int cmdQueueSize = 0;
#include "cmdq_decl.inc"                           // addCommandToQueue() with the header's default arguments

#include "cmdq_queue.inc"

// ---- helpers -------------------------------------------------------------
template <typename T, typename = void> struct HasDropCount : std::false_type {};
template <typename T> struct HasDropCount<T, std::void_t<decltype(std::declval<T&>().iCmdDropped)>> : std::true_type {};
template <typename T> static long dropCount(T& pic) {
  if constexpr (HasDropCount<T>::value) return (long)pic.iCmdDropped; else return -1;
}

static int g_fail = 0;
static void report(const char* id, bool ok, const char* what, const std::string& detail) {
  std::printf("CASE %s %s  %s [%s]\n", id, ok ? "pass" : "FAIL", what, detail.c_str());
  if (!ok) g_fail++;
}
static void add(const char* c, bool force) { addCommandToQueue(c, (int)std::strlen(c), force, 0); }
static void tick() { handleCommandQueue(); g_millis += 5000; }
static void answer(const char* r) { checkCommandResponse(r, (unsigned)std::strlen(r)); }
static const Pub* last(const char* topic) {
  for (auto it = g_pubs.rbegin(); it != g_pubs.rend(); ++it) if (it->topic == topic) return &*it;
  return nullptr;
}
static std::string pubs() {
  std::string s;
  for (auto& p : g_pubs) s += p.topic + "=" + p.payload + (p.retain ? "(r) " : " ");
  s = s.empty() ? "no publish" : s;
  return s + " (queue " + std::to_string(cmdQueueSize) + ", sent " + std::to_string(g_serial.size()) + ")";
}
// A new case starts 10 s after the last one: the queue paces sends on the last send and echo (TASK-1571).
static void resetQueue() { cmdQueueSize = 0; g_pubs.clear(); g_serial.clear(); g_millis += 10000; }

int main() {
  const char* kDropped = "otgw-firmware/command_dropped";
  const char* kFlag    = "otgw-firmware/command_retry_exceeded";

  // D1: PR=Z is never answered with its register letter, so it is retried and dropped.
  bool sawOn = false;
  resetQueue();
  add("PR=Z", true);
  for (int i = 0; i < 5; i++) tick();
  {
    const Pub* d = last(kDropped);
    const Pub* f = last(kFlag);
    const long n = dropCount(state.pic);
    bool ok = cmdQueueSize == 0 && g_serial.size() == 5 && d && d->payload == "PR=Z" && !d->retain
              && f && f->payload == "ON" && f->retain && (n == -1 || n == 1);
    sawOn = f && f->payload == "ON";
    report("D1", ok, "5 unanswered PR=Z: command_dropped PR=Z, flag ON retained, count 1",
           pubs() + " sent=" + std::to_string(g_serial.size()) + " count=" + std::to_string(n));
  }

  // D2: internal PR= and PM= confirmations leave the flag set.
  g_pubs.clear();
  add("PR=A", true); tick(); answer("PR: A=OpenTherm Gateway 6.8");
  add("PM=3", true); tick(); answer("PM: 3");
  report("D2", sawOn && last(kFlag) == nullptr && cmdQueueSize == 0,
         "after a drop (flag ON), confirmed PR= and PM= do not clear it", pubs());

  // D3: a confirmed command sent on purpose clears it.
  g_pubs.clear();
  add("TT=21.5", true); tick(); answer("TT: 21.50");
  {
    const Pub* f = last(kFlag);
    report("D3", f && f->payload == "OFF" && f->retain && cmdQueueSize == 0,
           "confirmed TT= sets the flag OFF (retained)", pubs());
  }

  // D4: TP= is appended, never replaced.
  resetQueue();
  add("TP=11:1", false); add("TP=12:1", false);
  report("D4", cmdQueueSize == 2 && std::strcmp(cmdqueue[0].cmd, "TP=11:1") == 0
                 && std::strcmp(cmdqueue[1].cmd, "TP=12:1") == 0,
         "two TP= back to back occupy two slots", std::to_string(cmdQueueSize) + " slot(s)");

  // C1: other commands keep the two-letter dedup.
  resetQueue();
  add("TT=20.5", false); add("TT=21.5", false);
  report("C1", cmdQueueSize == 1 && std::strcmp(cmdqueue[0].cmd, "TT=21.5") == 0,
         "two TT= back to back share one slot, last wins", std::to_string(cmdQueueSize) + " slot(s)");

  // C2: answered on the second try: no drop report.
  resetQueue();
  add("SB=15", true); tick(); tick(); answer("SB: 15.00");
  report("C2", cmdQueueSize == 0 && last(kDropped) == nullptr, "an answered command is not reported", pubs());

  // R1: an unknown command is refused with NG (TASK-1457).
  resetQueue();
  add("ZZ=1", true); tick(); answer("ZZ: NG");
  {
    const Pub* d = last(kDropped); const Pub* f = last(kFlag);
    report("R1", cmdQueueSize == 0 && g_serial.size() == 1 && d && d->payload == "ZZ=1 (NG)" && !d->retain
                   && f && f->payload == "ON" && f->retain,
           "ZZ: NG reports ZZ=1 (NG) after one send, flag ON retained", pubs());
  }

  // R2: another refusal while the flag is ON keeps it ON.
  g_pubs.clear();
  add("SB=99", true); tick(); answer("SB: OR");
  {
    const Pub* d = last(kDropped);
    report("R2", cmdQueueSize == 0 && d && d->payload == "SB=99 (OR)" && last(kFlag) == nullptr
                   && state.pic.bCmdRetryExceeded,
           "SB: OR is reported and does not set the flag OFF", pubs());
  }

  // R3: an unsupported PR= register is refused at once, not retried 5 times.
  resetQueue();
  add("PR=Q", true); tick(); answer("PR: BV");
  {
    const Pub* d = last(kDropped);
    report("R3", cmdQueueSize == 0 && g_serial.size() == 1 && d && d->payload == "PR=Q (BV)",
           "PR: BV reports PR=Q (BV) after one send", pubs());
  }

  // R4: a refused PM= is caught before the PM message-id match.
  resetQueue();
  add("PM=3", true); tick(); answer("PM: NG");
  {
    const Pub* d = last(kDropped);
    report("R4", cmdQueueSize == 0 && g_serial.size() == 1 && d && d->payload == "PM=3 (NG)",
           "PM: NG reports PM=3 (NG) after one send", pubs());
  }

  // ---- TASK-1555: a write is never dropped for a queued read when the queue is full ----
  auto fillReads = [](int from) {            // PM= is unique per value: each PM=<n> takes its own slot
    char c[8];
    for (int n = from; cmdQueueSize < CMDQUEUE_MAX; n++) { std::snprintf(c, sizeof c, "PM=%d", n); add(c, false); }
  };
  auto inQueue = [](const char* c) {
    for (int i = 0; i < cmdQueueSize; i++) if (std::strcmp(cmdqueue[i].cmd, c) == 0) return true;
    return false;
  };
  // Q1: a full queue of reads plus one HW= write keeps the write; the newest unsent read makes room.
  resetQueue(); fillReads(1);
  add("HW=1", false);
  report("Q1", cmdQueueSize == CMDQUEUE_MAX && inQueue("HW=1") && !inQueue("PM=20") && inQueue("PM=1"),
         "full of reads + HW=1: HW=1 kept, newest read PM=20 evicted", std::to_string(cmdQueueSize) + " slot(s)");
  // Q2: TP=<id>:<entry>=<value> writes a transparent slave parameter (OTGW-Commands.md TP); it keeps its place too.
  resetQueue(); fillReads(1);
  add("TP=11:12=40", false);
  report("Q2", inQueue("TP=11:12=40") && cmdQueueSize == CMDQUEUE_MAX, "full of reads + TP=11:12=40 (write): kept",
         std::to_string(cmdQueueSize) + " slot(s)");
  // Q3: a read already sent to the PIC (awaiting its answer) is not the one evicted.
  resetQueue(); add("PM=1", false); tick(); fillReads(2);
  add("CS=40.0", false);
  report("Q3", inQueue("CS=40.0") && inQueue("PM=1") && cmdqueue[0].retrycnt == 1,
         "the in-flight read PM=1 stays, an unsent read is evicted", std::to_string(cmdQueueSize) + " slot(s)");
  // Q7: a mixed queue (writes, one unsent read in the middle, writes): the write replaces that read, no write is lost.
  resetQueue();
  { char c[12]; for (int n = 0; n < 10; n++) { std::snprintf(c, sizeof c, "TT=%d", n); add(c, true); }
    add("PM=7", false);
    for (int n = 10; cmdQueueSize < CMDQUEUE_MAX; n++) { std::snprintf(c, sizeof c, "TT=%d", n); add(c, true); } }
  add("HW=1", false);
  {
    int tt = 0; for (int i = 0; i < cmdQueueSize; i++) if (std::strncmp(cmdqueue[i].cmd, "TT=", 3) == 0) tt++;
    report("Q7", inQueue("HW=1") && !inQueue("PM=7") && tt == CMDQUEUE_MAX - 1 && cmdQueueSize == CMDQUEUE_MAX
           && std::strcmp(cmdqueue[10].cmd, "TT=10") == 0 && std::strcmp(cmdqueue[CMDQUEUE_MAX - 1].cmd, "HW=1") == 0,
           "writes + one unsent PM=7 + writes, then HW=1: PM=7 leaves, HW=1 joins at the back (strict FIFO, TASK-1569), all 19 TT= stay in order",
           std::to_string(tt) + " TT=, slot10=" + cmdqueue[10].cmd + ", last=" + cmdqueue[CMDQUEUE_MAX - 1].cmd);
  }
  // Q4 (control): a read (TP=<id>:<entry>, and a lower-case pr=a) into a full queue of reads is still dropped.
  resetQueue(); fillReads(1);
  add("TP=11:1", false); add("pr=a", false);
  report("Q4", !inQueue("TP=11:1") && !inQueue("pr=a") && inQueue("PM=20") && cmdQueueSize == CMDQUEUE_MAX,
         "full of reads + TP=11:1 / pr=a (reads): dropped, queue unchanged", std::to_string(cmdQueueSize) + " slot(s)");
  // Q5 (control): a write into a full queue of writes is still dropped.
  resetQueue();
  { char c[12]; for (int n = 0; cmdQueueSize < CMDQUEUE_MAX; n++) { std::snprintf(c, sizeof c, "TT=%d", n); add(c, true); } }
  add("HW=1", false);
  report("Q5", !inQueue("HW=1") && cmdQueueSize == CMDQUEUE_MAX, "full of writes + HW=1: dropped (no read to evict)",
         std::to_string(cmdQueueSize) + " slot(s)");
  // Q6 (control): a queue that is not full is untouched: the write is appended, every read stays.
  resetQueue(); add("PM=1", false); add("PM=2", false); add("PR=A", false);
  add("HW=1", false);
  report("Q6", cmdQueueSize == 4 && inQueue("PM=1") && inQueue("PM=2") && inQueue("PR=A") && inQueue("HW=1"),
         "not full: HW=1 appended, no read evicted", std::to_string(cmdQueueSize) + " slot(s)");

  std::printf("%s (%d failed)\n", g_fail ? "FAIL" : "PASS", g_fail);
  return g_fail ? 1 : 0;
}
