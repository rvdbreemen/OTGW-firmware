// TASK-1736 host test: presized AsyncResponseStream for the polled endpoints.
//
// The sizing rule (restChooseStreamSize) and the presize table (kRestPresize)
// are sliced verbatim from src/OTGW-firmware/webServerCompat.h by
// build_and_run_stream_size.ps1; nothing is copied by hand.
//
// Part B models the library's growth rule (ESPAsyncWebServer 3.11.2
// WebResponses.cpp:930-932: on a write that does not fit, resizeAdd(exact
// shortfall)) to show the staircase on the old path and its absence on the
// presized path. The model counts resizes and the distinct block sizes they ask
// for; it does not model the heap allocator itself. The bodies are synthetic
// (sized from the emit code), not measured on a device.
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <set>
#include <vector>

#include "generated/stream_size_hint.inc"

static int g_fail = 0;
#define CHECK_EQ(a, b) do { const long long _a = (long long)(a), _b = (long long)(b); \
  if (_a != _b) { std::printf("FAIL %s:%d  %s == %lld, expected %lld\n", __FILE__, __LINE__, #a, _a, _b); g_fail++; } \
  else { std::printf("ok   %s == %lld\n", #a, _a); } } while (0)
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); g_fail++; } \
  else { std::printf("ok   %s\n", #c); } } while (0)

static const size_t DFLT = 1460;   // RESPONSE_STREAM_BUFFER_SIZE

// ---- Part B: model of AsyncResponseStream growth --------------------------------
struct StreamModel {
  size_t cap, used = 0, resizes = 0;
  std::set<size_t> sizes;          // distinct block sizes requested (initial + every resize)
  explicit StreamModel(size_t initial) : cap(initial) { sizes.insert(initial); }
  void write(size_t len) {
    const size_t room = cap - used;
    if (len > room) { cap += len - room; resizes++; sizes.insert(cap); }  // resizeAdd(shortfall)
    used += len;
  }
};

// Deterministic otmonitor-like body: 38 entries of small prints (1..21 B),
// about 3 KB in total. jitter shifts a few value widths, as live values do.
static std::vector<size_t> otmonitorWrites(unsigned jitter) {
  std::vector<size_t> w;
  unsigned seed = 12345u;
  for (int field = 0; field < 38; field++) {
    const size_t parts[] = {1, 1, 14, 2, 1, 1, 7, 2, 5u + ((field + jitter) % 3), 1, 1, 6, 2, 4, 1, 1, 7, 2, 10, 1};
    for (size_t p : parts) { seed = seed * 1103515245u + 12345u; w.push_back(p + ((seed >> 16) % 2)); }
  }
  return w;
}

static size_t total(const std::vector<size_t>& w) { size_t t = 0; for (size_t x : w) t += x; return t; }

int main() {
  std::printf("== A: sizing rule and table ==\n");
  CHECK_EQ(kRestPresize[REST_SIZE_NONE], 0);
  CHECK(kRestPresize[REST_SIZE_OTMONITOR]  > DFLT);
  CHECK(kRestPresize[REST_SIZE_BLE_ROSTER] > DFLT);
  CHECK(kRestPresize[REST_SIZE_WEATHER]    > DFLT);

  const size_t otm = kRestPresize[REST_SIZE_OTMONITOR];
  CHECK_EQ(restChooseStreamSize(0,    31732, DFLT), DFLT);   // unclassed
  CHECK_EQ(restChooseStreamSize(1024, 31732, DFLT), DFLT);   // presize below default: default
  CHECK_EQ(restChooseStreamSize(otm,  31732, DFLT), otm);    // healthy heap: presize
  CHECK_EQ(restChooseStreamSize(otm,  7668,  DFLT), DFLT);   // the TASK-1736 stuck state
  CHECK_EQ(restChooseStreamSize(otm,  otm + REST_PRESIZE_HEADROOM,     DFLT), otm);   // boundary
  CHECK_EQ(restChooseStreamSize(otm,  otm + REST_PRESIZE_HEADROOM - 1, DFLT), DFLT);

  std::printf("\n== B: staircase model (otmonitor-like body) ==\n");
  const std::vector<size_t> w0 = otmonitorWrites(0);
  const size_t len0 = total(w0);
  std::printf("     body %zu B in %zu writes\n", len0, w0.size());
  CHECK(len0 > DFLT);

  // Old path: every poll starts at 1460 and staircases.
  StreamModel oldp(DFLT);
  for (size_t x : w0) oldp.write(x);
  std::printf("     old path: %zu resizes, %zu distinct block sizes\n", oldp.resizes, oldp.sizes.size());
  CHECK(oldp.resizes > 100);          // RED on the defect: hundreds of recreates per response

  // New path: every poll presizes to the fixed otmonitor size.
  std::set<size_t> polledSizes;
  size_t resizes = 0;
  for (unsigned poll = 0; poll < 50; poll++) {
    const std::vector<size_t> w = otmonitorWrites(poll);    // jittered value widths
    StreamModel s(restChooseStreamSize(otm, 31732, DFLT));
    for (size_t x : w) s.write(x);
    resizes += s.resizes;
    polledSizes.insert(s.cap);
  }
  std::printf("     new path: %zu resizes over 50 polls, %zu distinct block size(s)\n",
              resizes, polledSizes.size());
  CHECK_EQ(resizes, 0);               // GREEN: no staircase
  CHECK_EQ(polledSizes.size(), 1);    // same block size every poll

  // Grow-past still works: a body longer than the presize grows (only the tail staircases).
  StreamModel over(restChooseStreamSize(otm, 31732, DFLT));
  for (int i = 0; i < 500; i++) over.write(10);   // 5000 B into a 4096 B buffer
  CHECK(over.used == 5000 && over.cap >= 5000);
  std::printf("     grow-past: %zu B body, presize %zu, %zu tail resizes\n", over.used, otm, over.resizes);

  std::printf("\n%s (%d failure(s))\n", g_fail ? "FAILED" : "ALL PASSED", g_fail);
  return g_fail ? 1 : 0;
}
