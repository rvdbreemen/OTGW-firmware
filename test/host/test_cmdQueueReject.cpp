//=======================================================================
// test_cmdQueueReject.cpp
//
// TASK-1442 host harness, driven by test_cmdQueueReject.py. Compiles the
// REAL 1.x command queue from OTGW-Core.ino (from "#define OTGW_CMD_RETRY"
// through checkOTGWcmdqueue(): the dropped-command reporting,
// removeFromCmdQueue, addOTWGcmdtoqueue, handleOTGWqueue and
// checkOTGWcmdqueue), sliced from the revision under test. Only I/O is
// doubled: PIC serial, MQTT (captured), WebSocket, debug, millis().
// state.otgw / state.debug carry the field names the firmware uses; they are
// the data shape, not logic.
//
// The defect: the PIC refuses a command with an error code ("ZZ: NG",
// "PR: BV"). checkOTGWcmdqueue() matched "ZZ: NG" on the two letters and
// removed the command as confirmed, so a refused command was never reported
// and even set command_retry_exceeded OFF.
//
// Cases (CASE <id> pass|FAIL):
//   R1  ZZ=1 answered "ZZ: NG" -> command_dropped "ZZ=1 (NG)" (retain 0),
//       flag ON (retain 1), cmd count 1, removed after one send
//   R2  with the flag ON, SB=99 answered "SB: OR" -> reported, flag not OFF
//   R3  PR=Q answered "PR: BV" -> reported at once as "PR=Q (BV)", not retried
//   C1  after the C3 drop, TT=21.5 answered "TT: 21.50" -> flag OFF, nothing dropped
//   C2  PR=S answered "PR: S=16.00" -> removed, nothing dropped
//   C3  PR=Z unanswered 5 times -> command_dropped "PR=Z" (the beta.8 path)
//=======================================================================

#include <cstdio>
#include <cstring>
#include <cstdint>
#include <cctype>
#include <string>
#include <vector>
#include <algorithm>

// ---- platform doubles -------------------------------------------------
class __FlashStringHelper;
#define F(s) (reinterpret_cast<const __FlashStringHelper*>(s))
#define PSTR(s) (s)
#define PROGMEM
#define pgm_read_byte(p) (*(const unsigned char*)(p))
#define CCONOFF(x) (x ? "ON" : "OFF")
#define snprintf_P snprintf
#define strcmp_P strcmp
using std::min;
static inline size_t strlcpy(char* d, const char* s, size_t n) {
  size_t l = std::strlen(s);
  if (n) { size_t c = l < n - 1 ? l : n - 1; std::memcpy(d, s, c); d[c] = '\0'; }
  return l;
}
#define strlcpy_P strlcpy

static uint32_t g_millis = 100000;
static uint32_t millis() { return g_millis; }

// Debug output is compiled out; the firmware macros (GCC statement
// expressions) only print.
#define OTGWDebugTln(...) ((void)0)
#define OTGWDebugln(...)  ((void)0)
#define OTGWDebugTf(...)  ((void)0)
#define OTGWDebugf(...)   ((void)0)
#define OTGWDebugT(...)   ((void)0)
#define OTGWDebug(...)    ((void)0)
#define OTGWDebugFlush()  ((void)0)

struct Pub { std::string topic, payload; bool retain; };
static std::vector<Pub> g_pubs;
bool sendMQTTData(const __FlashStringHelper* topic, const char* value, const bool retain = false) {
  g_pubs.push_back({reinterpret_cast<const char*>(topic), value, retain});
  return true;
}
static std::vector<std::string> g_serial;
static void sendOTGW(const char* cmd, int len) { g_serial.emplace_back(cmd, (size_t)len); }
static void sendEventToWebSocket(char, const char*) {}
static void triggerPICsettingsReadout() {}
static bool isPICEnabled() { return true; }
static char cMsg[128];

struct OtgwDouble { uint32_t iCmdDropped = 0; bool bCmdRetryExceeded = false; };
struct DebugDouble { bool bOTmsg = false; };
struct StateDouble { OtgwDouble otgw; DebugDouble debug; };
static StateDouble state;

struct OT_cmd_t { char cmd[15]; uint8_t cmdlen; uint8_t retrycnt; unsigned long due; };
#define CMDQUEUE_MAX 20
struct OT_cmd_t cmdqueue[CMDQUEUE_MAX];
int cmdQueueSize = 0;
void addOTWGcmdtoqueue(const char* buf, const int len, const bool forceQueue = false, const int16_t delay = 1000);

// ---- sliced firmware (revision under test) -----------------------------
#include "cmdq_reject.inc"

// ---- helpers -------------------------------------------------------------
static int g_fail = 0;
static void report(const char* id, bool ok, const char* what, const std::string& detail) {
  std::printf("CASE %s %s  %s [%s]\n", id, ok ? "pass" : "FAIL", what, detail.c_str());
  if (!ok) g_fail++;
}
static void add(const char* c) { addOTWGcmdtoqueue(c, (int)std::strlen(c), true, 0); }
static void tick() { handleOTGWqueue(); g_millis += 5000; }
static void answer(const char* r) { checkOTGWcmdqueue(r, (unsigned)std::strlen(r)); }
static const Pub* last(const char* topic) {
  for (auto it = g_pubs.rbegin(); it != g_pubs.rend(); ++it) if (it->topic == topic) return &*it;
  return nullptr;
}
static std::string pubs() {
  std::string s;
  for (auto& p : g_pubs) s += p.topic + "=" + p.payload + (p.retain ? "(r) " : " ");
  return s.empty() ? "no publish" : s;
}
static void reset() { cmdQueueSize = 0; g_pubs.clear(); g_serial.clear(); }

int main() {
  const char* kDropped = "otgw-firmware/command_dropped";
  const char* kFlag    = "otgw-firmware/command_retry_exceeded";

  // R1: an unknown command is refused with NG.
  reset();
  add("ZZ=1"); tick(); answer("ZZ: NG");
  {
    const Pub* d = last(kDropped); const Pub* f = last(kFlag);
    bool ok = cmdQueueSize == 0 && g_serial.size() == 1 && d && d->payload == "ZZ=1 (NG)" && !d->retain
              && f && f->payload == "ON" && f->retain && state.otgw.iCmdDropped == 1;
    report("R1", ok, "ZZ: NG reports ZZ=1 (NG), flag ON retained, count 1",
           pubs() + " queue=" + std::to_string(cmdQueueSize) + " count=" + std::to_string(state.otgw.iCmdDropped));
  }

  // R2: another refusal while the flag is ON keeps it ON.
  g_pubs.clear();
  add("SB=99"); tick(); answer("SB: OR");
  {
    const Pub* d = last(kDropped); const Pub* f = last(kFlag);
    bool ok = cmdQueueSize == 0 && d && d->payload == "SB=99 (OR)" && f == nullptr && state.otgw.bCmdRetryExceeded;
    report("R2", ok, "SB: OR is reported and does not set the flag OFF", pubs());
  }

  // R3: an unsupported PR= register is refused at once, not retried 5 times.
  reset();
  add("PR=Q"); tick(); answer("PR: BV");
  {
    const Pub* d = last(kDropped);
    report("R3", cmdQueueSize == 0 && g_serial.size() == 1 && d && d->payload == "PR=Q (BV)",
           "PR: BV reports PR=Q (BV) after one send", pubs() + " queue=" + std::to_string(cmdQueueSize));
  }

  // C2: a PR= readout answer still matches its register.
  reset();
  add("PR=S"); tick(); answer("PR: S=16.00");
  report("C2", cmdQueueSize == 0 && last(kDropped) == nullptr, "PR: S=16.00 confirms PR=S", pubs());

  // C3: no answer at all is still dropped after 5 tries.
  reset();
  add("PR=Z");
  for (int i = 0; i < 5; i++) tick();
  {
    const Pub* d = last(kDropped);
    report("C3", cmdQueueSize == 0 && g_serial.size() == 5 && d && d->payload == "PR=Z",
           "unanswered PR=Z is dropped after 5 tries", pubs());
  }

  // C1: the C3 drop left the flag ON; a real confirmation clears it.
  g_pubs.clear();
  add("TT=21.5"); tick(); answer("TT: 21.50");
  {
    const Pub* f = last(kFlag);
    report("C1", cmdQueueSize == 0 && last(kDropped) == nullptr && f && f->payload == "OFF",
           "after a drop, TT: 21.50 confirms, flag OFF, nothing dropped", pubs());
  }

  std::printf("%s (%d failed)\n", g_fail ? "FAIL" : "PASS", g_fail);
  return g_fail ? 1 : 0;
}
