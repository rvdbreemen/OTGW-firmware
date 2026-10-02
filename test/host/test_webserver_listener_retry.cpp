// TASK-1130 host check: the port-80 listener comes back after a refused bind.
//
// test_webserver_listener_retry.py extracts, from the revision under test:
//   tail.inc      the statements between server.begin() and the "Set up first
//                 message" comment in startWebserver() (FSexplorer.ino)
//   listener.inc  handleWebserverListener() (FSexplorer.ino)
//   tick.inc      the lines of doBackgroundTasks() (OTGW-firmware.ino) that
//                 declare and fire the listener timer
//   safeTimers.h  as is
// Before TASK-1130 the three .inc files are empty: the firmware had no check
// and no retry, and that absence is what the OLD run exercises.
//
// Only the AsyncWebServer boundary and the lwIP bind are modelled. begin()
// binds once the simulated clock passes the TIME_WAIT expiry (before that
// tcp_bind() returns ERR_USE and the pcb is closed), and it returns early while
// the server already listens, as AsyncServer::begin() does when it holds a pcb.
//
// One case per process (argv[1]): the timer and the retry counter are
// function-local statics, so every case starts from a fresh boot.
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

typedef uint8_t byte;
static uint32_t g_now = 0;
static uint32_t millis() { return g_now; }
static long random(long lo, long) { return lo; }
#include "safeTimers.h"

#define F(s) (s)
#define PSTR(s) (s)
static std::vector<std::string> g_log;
static void DebugTln(const char* s) { g_log.push_back(s); }
static void DebugTf(const char* fmt, ...) {
  char buf[256];
  va_list ap;
  va_start(ap, fmt);
  std::vsnprintf(buf, sizeof buf, fmt, ap);
  va_end(ap);
  g_log.push_back(buf);
}

enum tcp_state { CLOSED = 0, LISTEN = 1 };
static uint32_t g_bindOkAt = 0;   // lwIP refuses the bind on port 80 before this time
struct FakeWebServer {
  tcp_state st = CLOSED;
  size_t begins = 0;
  void begin() {
    begins++;
    if (st == LISTEN) return;
    st = (g_now >= g_bindOkAt) ? LISTEN : CLOSED;
  }
  tcp_state state() const { return st; }
};
static FakeWebServer server;

#include "listener.inc"

static void startWebserverTail() {
  server.begin();
#include "tail.inc"
}

static void loopTick() {
#include "tick.inc"
}

static const uint32_t T_START = 15000;    // startWebserver() runs 15 s after the portal's last connection closed
static const uint32_t T_TIMEWAIT = 120000; // 2*TCP_MSL from that close: the bind is refused until then
static const uint32_t T_END = 400000;
static const uint32_t STEP = 100;          // one doBackgroundTasks() turn every 100 ms

struct Run {
  uint32_t firstListen = 0, secondListen = 0;
  size_t beginsAtFirstListen = 0, beginsAtLose = 0, beginsAtSecondListen = 0;
};

// The provisioning boot: startWebserver() at T_START, then the loop. With
// loseAt set, the listener is lost at that time and the bind is refused again
// until rebindOkAt.
static Run simulate(uint32_t bindOkAt, uint32_t loseAt = 0, uint32_t rebindOkAt = 0) {
  Run r;
  g_now = T_START;
  g_bindOkAt = bindOkAt;
  startWebserverTail();
  tcp_state prev = server.state();
  if (prev == LISTEN) { r.firstListen = g_now; r.beginsAtFirstListen = server.begins; }
  bool lost = false;
  for (g_now = T_START + STEP; g_now <= T_END; g_now += STEP) {
    if (loseAt && !lost && g_now >= loseAt) {
      server.st = CLOSED;
      g_bindOkAt = rebindOkAt;
      lost = true;
      r.beginsAtLose = server.begins;
      prev = CLOSED;
    }
    loopTick();
    tcp_state cur = server.state();
    if (cur == LISTEN && prev != LISTEN) {
      if (!r.firstListen) { r.firstListen = g_now; r.beginsAtFirstListen = server.begins; }
      else if (lost && !r.secondListen) { r.secondListen = g_now; r.beginsAtSecondListen = server.begins; }
    }
    prev = cur;
  }
  return r;
}

// The retry counts reported by "listening on port 80 after N retries" lines.
static std::vector<long> listeningCounts() {
  std::vector<long> n;
  for (auto& l : g_log) {
    const char* p = std::strstr(l.c_str(), "listening on port 80 after ");
    if (p) n.push_back(std::strtol(p + std::strlen("listening on port 80 after "), nullptr, 10));
  }
  return n;
}
static size_t linesWith(const char* needle) {
  size_t c = 0;
  for (auto& l : g_log) if (std::strstr(l.c_str(), needle)) c++;
  return c;
}
static std::string joinCounts(const std::vector<long>& v) {
  std::string s;
  for (long x : v) s += (s.empty() ? "" : ",") + std::to_string(x);
  return s.empty() ? "none" : s;
}
static int verdict(const char* id, const char* title, bool ok, const std::string& got) {
  std::printf("CASE %s %s\n     %s\n     got : %s\n", id, ok ? "pass" : "FAIL", title, got.c_str());
  return ok ? 0 : 1;
}

int main(int argc, char** argv) {
  const std::string id = argc > 1 ? argv[1] : "";
  char got[320];
  if (id == "L1") {
    Run r = simulate(T_TIMEWAIT);
    auto n = listeningCounts();
    size_t retries = r.firstListen ? r.beginsAtFirstListen - 1 : 0;
    bool ok = r.firstListen && n.size() == 1 && (size_t)n[0] == retries && server.begins == r.beginsAtFirstListen;
    std::snprintf(got, sizeof got, "listening at %s, begin() calls %zu, 'listening' lines report %s, begin() calls after listening %zu",
                  r.firstListen ? (std::to_string(r.firstListen / 1000.0).substr(0, 5) + " s").c_str() : "never",
                  server.begins, joinCounts(n).c_str(), r.firstListen ? server.begins - r.beginsAtFirstListen : 0);
    return verdict("L1", "provisioning boot: the listener comes back after TIME_WAIT, the recovery is logged once with the true retry count, and begin() stops once listening", ok, got);
  }
  if (id == "L2") {
    Run r = simulate(0);
    size_t port80 = linesWith("port 80");
    bool ok = r.firstListen == T_START && server.begins == 1 && port80 == 0;
    std::snprintf(got, sizeof got, "listening from startWebserver(): %s, begin() calls %zu, port-80 log lines %zu",
                  r.firstListen == T_START ? "yes" : "no", server.begins, port80);
    return verdict("L2", "steady state: a server that listens from the start is left alone (one begin(), no port-80 log)", ok, got);
  }
  if (id == "L3") {
    Run r = simulate(T_TIMEWAIT, 200000, 212000);
    auto n = listeningCounts();
    size_t second = r.secondListen ? r.beginsAtSecondListen - r.beginsAtLose : 0;
    bool ok = r.secondListen && n.size() == 2 && (size_t)n[1] == second;
    std::snprintf(got, sizeof got, "second recovery at %s after %zu begin() calls, 'listening' lines report %s",
                  r.secondListen ? (std::to_string(r.secondListen / 1000.0).substr(0, 5) + " s").c_str() : "never",
                  second, joinCounts(n).c_str());
    return verdict("L3", "a second outage is retried again and its log counts its own retries from zero", ok, got);
  }
  if (id == "L4") {
    g_now = T_START;
    g_bindOkAt = T_TIMEWAIT;
    startWebserverTail();
    size_t c = linesWith("bind on port 80 failed");
    std::snprintf(got, sizeof got, "state after begin() %s, 'bind on port 80 failed' lines %zu",
                  server.state() == LISTEN ? "LISTEN" : "CLOSED", c);
    return verdict("L4", "a refused first bind in startWebserver() is logged, not silent", c == 1, got);
  }
  if (id == "L5") {
    Run r = simulate(T_TIMEWAIT);
    bool ok = r.firstListen && r.firstListen <= T_TIMEWAIT + 5000 + STEP;
    std::snprintf(got, sizeof got, "bind possible from %.1f s, listening at %s", T_TIMEWAIT / 1000.0,
                  r.firstListen ? (std::to_string(r.firstListen / 1000.0).substr(0, 5) + " s").c_str() : "never");
    return verdict("L5", "the listener is back within one 5 s retry period after the bind becomes possible", ok, got);
  }
  std::printf("unknown case '%s'\n", id.c_str());
  return 2;
}
