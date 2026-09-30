/*
***************************************************************************
**  Program  : test/host/test_rate_limit_gcra.cpp
**
**  Host harness for the REST poll-budget limiter (ADR-172) in
**  src/OTGW-firmware/restAPI.ino: checkApiRateLimit(), rateLimitTryAdmit()
**  and sendApiRateLimited(), with the real route table and budget initialiser.
**
**  Nothing under test is copied here. rate_limit_gcra.ps1 slices the
**  declarations and functions out of restAPI.ino by anchor into
**  generated/rate_limit_under_test.inc, which this file includes. This file
**  only supplies the platform stubs the slices call (arduino_shim.h, the web
**  response capture) and a millis() the test controls. Every request goes
**  through the real checkApiRateLimit(), so the route lookup, the shared
**  otmonitor/telegraf budget and the RFC 9457 429 are the shipped code.
**
**  Two groups of checks:
**    - normal operation: burst, headers, alias, sustained rates, a stream
**      across the millis() wrap. These hold before and after the TASK-1037 D1
**      fix. A deterministic random stream prints a checksum; old and fix must
**      print the same one.
**    - D1: the first request after a long idle must be admitted. Before the
**      fix, an idle gap over 2^31 ms (24.86 days) was read as a budget far in
**      the future and refused for up to 24.8 more days.
**
**  Run:  powershell -NoProfile -ExecutionPolicy Bypass -File test/host/rate_limit_gcra.ps1 [-Rev <git rev>]
**
**  TERMS OF USE: GNU GPLv3. See OTGW-firmware.h for the full notice.
***************************************************************************
*/
#include "arduino_shim.h"

#include <cstdarg>
#include <utility>
#include <vector>

// ---- platform surface the slices touch ---------------------------------------
#ifndef memcpy_P
#define memcpy_P memcpy
#endif

// The clock under test. checkApiRateLimit() reads millis() itself.
static uint32_t g_nowMs = 0;
static uint32_t millis() { return g_nowMs; }

// Only HTTP_GET is limited; any other value stands for a mutation.
enum HTTPMethod { HTTP_GET = 1, HTTP_POST = 2 };

// Response capture. Signatures mirror webServerCompat.h, so overload
// resolution for the F() arguments matches the device build.
struct CapturedResponse {
  int code = 0;
  std::string contentType;
  std::string body;
  std::vector<std::pair<std::string, std::string>> headers;
  bool cors = false;
};
static CapturedResponse g_resp;

static const char* flashStr(const __FlashStringHelper* p) { return reinterpret_cast<const char*>(p); }
static void webPushHeader(const char* name, const char* value) { g_resp.headers.emplace_back(name, value); }
static void webPushHeader(const __FlashStringHelper* name, const __FlashStringHelper* value) { webPushHeader(flashStr(name), flashStr(value)); }
static void webPushHeader(const __FlashStringHelper* name, const char* value) { webPushHeader(flashStr(name), value); }
static void webSend(int code, const __FlashStringHelper* contentType, const char* body) {
  g_resp.code = code;
  g_resp.contentType = flashStr(contentType);
  g_resp.body = body;
}
static void sendCorsOriginHeader() { g_resp.cors = true; }

// ---- code under test, sliced by anchor from restAPI.ino ----------------------
#include "generated/rate_limit_under_test.inc"

// ---- test support ----------------------------------------------------------
static int g_pass = 0, g_fail = 0;

static std::string fmt(const char* f, ...) {
  char buf[640];
  va_list ap;
  va_start(ap, f);
  vsnprintf(buf, sizeof(buf), f, ap);
  va_end(ap);
  return buf;
}

static void check(bool ok, const std::string& what) {
  printf("  %s  %s\n", ok ? "PASS" : "FAIL", what.c_str());
  if (ok) g_pass++; else g_fail++;
}

static ApiRateLimitBudget g_bootBudgets[RL_BUDGET_COUNT];
static void resetBudgets() { memcpy(gApiRateLimitBudgets, g_bootBudgets, sizeof(g_bootBudgets)); }

struct Endpoint { const char* resource; const char* sub; const char* name; uint8_t budget; };
static const Endpoint OTMONITOR = { "otgw",   "otmonitor", "otmonitor",   RL_BUDGET_OTMONITOR };
static const Endpoint TELEGRAF  = { "otgw",   "telegraf",  "telegraf",    RL_BUDGET_OTMONITOR };
static const Endpoint DEVTIME   = { "device", "time",      "device/time", RL_BUDGET_DEVICE_TIME };
static const Endpoint DEVINFO   = { "device", "info",      "device/info", RL_BUDGET_COUNT };  // no budget

// One request for /api/v2/<resource>/<sub> at time atMs, through the real gate.
// Returns 200 when admitted, else the status the limiter answered with.
static int request(const Endpoint& ep, uint32_t atMs, HTTPMethod method = HTTP_GET) {
  g_nowMs = atMs;
  g_resp = CapturedResponse();
  restResponseStatus = 0;
  char words[5][API_WORD_LEN] = { "", "api", "v2", "", "" };
  strncpy(words[3], ep.resource, API_WORD_LEN - 1);
  strncpy(words[4], ep.sub, API_WORD_LEN - 1);
  if (checkApiRateLimit(words, 5, method)) return 200;
  return g_resp.code;
}

static std::string header(const char* name) {
  for (const auto& h : g_resp.headers) if (h.first == name) return h.second;
  return std::string("(absent)");
}

static const uint32_t DAY_MS = 86400000UL;
static const uint32_t T0     = 1000000UL;  // 1000 s after boot: an unremarkable start

// After a refusal at `at`, step simulated time forward until a request is
// admitted and return the elapsed ms (0 if still refused after maxSteps).
// A refusal does not move the budget, so probing does not change what it measures.
static uint64_t refusalLengthMs(const Endpoint& ep, uint32_t at, uint32_t stepMs, uint32_t maxSteps) {
  for (uint32_t k = 1; k <= maxSteps; k++) {
    if (request(ep, at + k * stepMs) == 200) return (uint64_t)k * stepMs;
  }
  return 0;
}

// ---- normal operation --------------------------------------------------------
static void testBurstAndResponse() {
  resetBudgets();
  const int a = request(OTMONITOR, T0), b = request(OTMONITOR, T0), c = request(OTMONITOR, T0);
  check(a == 200 && b == 200 && c == 429,
        fmt("otmonitor, 3 GETs in one millisecond -> %d %d %d (want 200 200 429)", a, b, c));
  check(restResponseStatus == 429 && g_resp.code == 429,
        fmt("the 429 is recorded and sent (restResponseStatus %d, sent %d)", restResponseStatus, g_resp.code));
  check(g_resp.contentType == "application/problem+json",
        "content type application/problem+json (got " + g_resp.contentType + ")");
  check(header("Retry-After") == "2", "Retry-After: 2 (got " + header("Retry-After") + ")");
  check(header("Cache-Control") == "no-store", "Cache-Control: no-store (got " + header("Cache-Control") + ")");
  check(header("RateLimit") == "\"poll\";r=0;t=2", "RateLimit: \"poll\";r=0;t=2 (got " + header("RateLimit") + ")");
  check(header("RateLimit-Policy") == "\"poll\";q=2;w=2",
        "RateLimit-Policy: \"poll\";q=2;w=2 (got " + header("RateLimit-Policy") + ")");
  check(g_resp.body.find("\"retry_after\":2,") != std::string::npos, "body carries \"retry_after\":2");
  check(g_resp.cors, "CORS origin header staged");
}

static void testAliasSharesBudget() {
  resetBudgets();
  int a = request(OTMONITOR, T0), b = request(OTMONITOR, T0), c = request(TELEGRAF, T0);
  check(a == 200 && b == 200 && c == 429,
        fmt("otmonitor x2 then telegraf in one ms -> %d %d %d (want 200 200 429, one budget)", a, b, c));
  resetBudgets();
  a = request(TELEGRAF, T0); b = request(TELEGRAF, T0); c = request(OTMONITOR, T0);
  check(a == 200 && b == 200 && c == 429,
        fmt("telegraf x2 then otmonitor in one ms -> %d %d %d (want 200 200 429, one budget)", a, b, c));
}

static void testDeviceTimeBudget() {
  resetBudgets();
  const int a = request(DEVTIME, T0), b = request(DEVTIME, T0), c = request(DEVTIME, T0);
  check(a == 200 && b == 200 && c == 429,
        fmt("device/time, 3 GETs in one millisecond -> %d %d %d (want 200 200 429)", a, b, c));
  check(header("Retry-After") == "4", "device/time Retry-After: 4 (got " + header("Retry-After") + ")");
  check(header("RateLimit-Policy") == "\"poll\";q=2;w=4",
        "device/time RateLimit-Policy: \"poll\";q=2;w=4 (got " + header("RateLimit-Policy") + ")");
  check(request(OTMONITOR, T0) == 200, "otmonitor is still admitted while device/time is exhausted");
}

static void testNotLimited() {
  resetBudgets();
  int refused = 0;
  for (int i = 0; i < 10; i++) if (request(DEVINFO, T0) != 200) refused++;
  check(refused == 0, fmt("device/info has no budget: 10 GETs in one ms, %d refused (want 0)", refused));
  refused = 0;
  for (int i = 0; i < 5; i++) if (request(OTMONITOR, T0, HTTP_POST) != 200) refused++;
  check(refused == 0, fmt("non-GET on otgw/otmonitor is never limited: 5 in one ms, %d refused (want 0)", refused));
}

static int admittedInStream(const Endpoint& ep, uint32_t start, uint32_t stepMs, int n) {
  resetBudgets();
  int admitted = 0;
  for (int i = 0; i < n; i++) if (request(ep, start + (uint32_t)i * stepMs) == 200) admitted++;
  return admitted;
}

static void testSustainedRates() {
  int adm = admittedInStream(OTMONITOR, T0, 1000, 120);
  check(adm == 81, fmt("otmonitor at 1 req/s for 120 s: %d admitted (want 81: 120 s / 1.5 s + 1 burst token)", adm));
  adm = admittedInStream(OTMONITOR, T0, 1600, 76);
  check(adm == 76, fmt("otmonitor at 1 req per 1.6 s for 121.6 s: %d of 76 admitted (want all)", adm));
  adm = admittedInStream(OTMONITOR, T0, 2000, 300);
  check(adm == 300, fmt("otmonitor at the UI period (2 s) for 10 min: %d of 300 admitted (want all)", adm));
  adm = admittedInStream(DEVTIME, T0, 5000, 120);
  check(adm == 120, fmt("device/time at the UI period (5 s) for 10 min: %d of 120 admitted (want all)", adm));
}

static std::string admitPattern(const Endpoint& ep, uint32_t start) {
  resetBudgets();
  std::string s;
  for (int i = 0; i < 120; i++) s += (request(ep, start + (uint32_t)i * 1000u) == 200) ? 'A' : 'r';
  return s;
}

static void testAcrossTheWrap() {
  // The second stream starts 60 s before millis() wraps to 0.
  const uint32_t nearWrap = 0xFFFFFFFFu - 59999u;
  check(admitPattern(OTMONITOR, T0) == admitPattern(OTMONITOR, nearWrap),
        "otmonitor at 1 req/s across the millis() wrap admits exactly as far from it");
  check(admitPattern(DEVTIME, T0) == admitPattern(DEVTIME, nearWrap),
        "device/time at 1 req/s across the millis() wrap admits exactly as far from it");
}

// Deterministic mixed stream with short gaps and rare idles of up to 8 days,
// crossing the millis() wrap. Even three idles in a row stay under 2^31 ms, the
// range where the pre-fix arithmetic was already right, so the checksum must not
// change with the fix.
static void differentialStream() {
  uint32_t x = 0x2545F491u;
  auto rnd = [&x]() { x ^= x << 13; x ^= x >> 17; x ^= x << 5; return x; };
  const Endpoint* eps[4] = { &OTMONITOR, &TELEGRAF, &DEVTIME, &DEVINFO };
  uint64_t h = 1469598103934665603ULL;                       // FNV-1a 64
  auto mix = [&h](uint32_t v) { for (int i = 0; i < 4; i++) { h ^= (v >> (8 * i)) & 0xFFu; h *= 1099511628211ULL; } };

  resetBudgets();
  uint32_t t = 0xFFFFFFFFu - 600000u;                        // 10 min before the first wrap
  unsigned wraps = 0, admitted = 0, refused = 0, longIdles = 0;
  for (int i = 0; i < 600000; i++) {
    const uint32_t r = rnd();
    uint32_t gap;
    if ((r & 0x3FFFu) == 0) { gap = rnd() % (8u * DAY_MS); longIdles++; }
    else                    { gap = rnd() % 4000u; }
    const uint32_t prev = t;
    t += gap;
    if (t < prev) wraps++;
    const int st = request(*eps[(r >> 16) & 3u], t);
    mix((uint32_t)st);
    if (st == 200) { admitted++; mix(0); }
    else { refused++; mix((uint32_t)strtoul(header("Retry-After").c_str(), nullptr, 10)); }
  }
  printf("  INFO  mixed stream: 600000 GETs, %u long idles, %u millis() wraps, %u admitted, %u refused\n",
         longIdles, wraps, admitted, refused);
  printf("  INFO  stream checksum %016llx (old and fix must print the same value)\n", (unsigned long long)h);
  check(wraps >= 2, fmt("the stream crosses the millis() wrap at least twice (%u)", wraps));
}

// ---- D1: long idle -------------------------------------------------------------
struct Idle { const char* label; uint32_t ms; };
static const Idle kIdles[] = {
  { "1 d",                  86400000UL },
  { "24.8 d",             2142720000UL },
  { "2^31 ms (24.855 d)", 2147483648UL },
  { "2^31 + 1 ms",        2147483649UL },
  { "24.9 d",             2151360000UL },
  { "30 d",               2592000000UL },
  { "49 d",               4233600000UL },
};

static void testLongIdle() {
  const Endpoint* eps[2] = { &OTMONITOR, &DEVTIME };
  for (const Endpoint* ep : eps) {
    for (int prior = 1; prior <= 2; prior++) {
      for (const Idle& idle : kIdles) {
        resetBudgets();
        bool primedOk = true;
        for (int k = 0; k < prior; k++) primedOk = primedOk && (request(*ep, T0) == 200);
        const uint32_t at = T0 + idle.ms;                     // modulo 2^32, like millis()
        const int st = request(*ep, at);
        std::string what = fmt("idle %-18s %-11s after %d GET(s): first request -> %d",
                               idle.label, ep->name, prior, st);
        if (st != 200) {
          const std::string ra = header("Retry-After");
          // 1-minute probes for up to 40 days: past any possible pre-fix refusal
          // (under 24.86 d), and k * step stays below 2^32.
          const uint64_t len = refusalLengthMs(*ep, at, 60000u, 40u * 1440u);
          what += fmt(", Retry-After %s s (%.2f d), refused for %.2f d of simulated time",
                      ra.c_str(), strtod(ra.c_str(), nullptr) / 86400.0, (double)len / DAY_MS);
        }
        check(primedOk && st == 200, what);
      }
    }
  }
}

// An idle gap within burst * window of 2^32 ms looks like a legitimate lead.
// It may cost a refusal, but never more than one window.
static void testResidualBand() {
  const Endpoint* eps[2] = { &OTMONITOR, &DEVTIME };
  const uint32_t idle = (uint32_t)(4294967296ULL - 1000ULL);
  for (const Endpoint* ep : eps) {
    const uint32_t window = g_bootBudgets[ep->budget].windowMs;
    for (int prior = 1; prior <= 2; prior++) {
      resetBudgets();
      for (int k = 0; k < prior; k++) request(*ep, T0);
      const uint32_t at = T0 + idle;
      const int st = request(*ep, at);
      const uint64_t len = (st == 200) ? 0 : refusalLengthMs(*ep, at, 1u, 20000u);
      check(st == 200 || (len > 0 && len <= window),
            fmt("idle 2^32 - 1 s        %-11s after %d GET(s): first request -> %d, refused for %llu ms (want <= one window, %lu ms)",
                ep->name, prior, st, (unsigned long long)len, (unsigned long)window));
    }
  }
}

int main() {
  memcpy(g_bootBudgets, gApiRateLimitBudgets, sizeof(g_bootBudgets));
  printf("== budgets from the shipped initialiser ==\n");
  printf("  otmonitor + telegraf: window %lu ms, burst %u\n",
         (unsigned long)g_bootBudgets[RL_BUDGET_OTMONITOR].windowMs, (unsigned)g_bootBudgets[RL_BUDGET_OTMONITOR].burstTokens);
  printf("  device/time         : window %lu ms, burst %u\n",
         (unsigned long)g_bootBudgets[RL_BUDGET_DEVICE_TIME].windowMs, (unsigned)g_bootBudgets[RL_BUDGET_DEVICE_TIME].burstTokens);

  printf("\n== normal operation (holds before and after the D1 fix) ==\n");
  testBurstAndResponse();
  testAliasSharesBudget();
  testDeviceTimeBudget();
  testNotLimited();
  testSustainedRates();
  testAcrossTheWrap();
  differentialStream();

  printf("\n== D1: the first request after a long idle is admitted ==\n");
  testLongIdle();
  testResidualBand();

  printf("\n%d passed, %d failed\n", g_pass, g_fail);
  return g_fail ? 1 : 0;
}
