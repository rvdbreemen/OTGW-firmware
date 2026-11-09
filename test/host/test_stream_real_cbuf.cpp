// TASK-1736 AC2 host test: the REAL AsyncResponseStream write path on the REAL cbuf.
//
// Code under test, nothing re-implemented:
//   - cbuf.cpp from the Arduino-ESP32 core (compiled as is, from the PlatformIO package);
//   - AsyncResponseStream::write(const uint8_t*, size_t), sliced verbatim from
//     ESPAsyncWebServer 3.11.2 src/WebResponses.cpp into StreamUnderTest::write;
//   - restChooseStreamSize() and kRestPresize[], sliced verbatim from
//     src/OTGW-firmware/webServerCompat.h (the presize decision the firmware makes).
// Faked: the ESP-IDF byte ring buffer under cbuf (fake_rtos/freertos/ringbuf.h) and
// malloc/free inside cbuf.cpp, both only to count allocations. The heap allocator itself
// (TLSF on the device) is not modelled: the test counts what is asked of it.
//
// RED on the defect (the old path: a 1460 B stream that grows by the exact shortfall on
// every small JsonEmit print), GREEN on the presize. Set OTGW_NO_PRESIZE=1 to run the
// "green" part with the presize disabled and see it fail.
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <set>
#include <vector>

#include "cbuf.h"
#include "generated/stream_size_hint.inc"

// ---- allocation tracker ---------------------------------------------------------
struct Tracker {
  size_t ringCreates = 0, ringDeletes = 0, tempMallocs = 0;
  size_t live = 0, peak = 0;
  std::set<size_t> ringSizes;
  void add(size_t n) { live += n; if (live > peak) peak = live; }
  void sub(size_t n) { live -= n; }
  void reset() { *this = Tracker(); }
};
static Tracker T;

struct Block { size_t n; };
extern "C" void *track_malloc(size_t n) {
  Block *b = (Block *)std::malloc(sizeof(Block) + n);
  if (!b) return nullptr;
  b->n = n; T.tempMallocs++; T.add(n);
  return b + 1;
}
extern "C" void track_free(void *p) {
  if (!p) return;
  Block *b = (Block *)p - 1;
  T.sub(b->n); std::free(b);
}

// ---- fake ESP-IDF byte ring buffer -------------------------------------------------
struct Ring { size_t cap; std::vector<uint8_t> data; std::vector<uint8_t> item; };
extern "C" RingbufHandle_t xRingbufferCreate(size_t size, RingbufferType_t) {
  Ring *r = new Ring(); r->cap = size;
  T.ringCreates++; T.ringSizes.insert(size); T.add(size);
  return r;
}
extern "C" void vRingbufferDelete(RingbufHandle_t h) {
  Ring *r = (Ring *)h; T.ringDeletes++; T.sub(r->cap); delete r;
}
extern "C" size_t xRingbufferGetMaxItemSize(RingbufHandle_t h) { return ((Ring *)h)->cap; }
extern "C" size_t xRingbufferGetCurFreeSize(RingbufHandle_t h) { Ring *r = (Ring *)h; return r->cap - r->data.size(); }
extern "C" void *xRingbufferReceiveUpTo(RingbufHandle_t h, size_t *got, TickType_t, size_t max) {
  Ring *r = (Ring *)h; size_t n = r->data.size() < max ? r->data.size() : max;
  if (!n) { *got = 0; return nullptr; }
  r->item.assign(r->data.begin(), r->data.begin() + n);
  r->data.erase(r->data.begin(), r->data.begin() + n);
  *got = n; return r->item.data();
}
extern "C" void vRingbufferReturnItem(RingbufHandle_t, void *) {}
extern "C" BaseType_t xRingbufferSend(RingbufHandle_t h, const void *p, size_t n, TickType_t) {
  Ring *r = (Ring *)h;
  if (n > r->cap - r->data.size()) return pdFALSE;
  const uint8_t *b = (const uint8_t *)p; r->data.insert(r->data.end(), b, b + n);
  return pdTRUE;
}
extern "C" void vRingbufferGetInfo(RingbufHandle_t h, UBaseType_t *, UBaseType_t *, UBaseType_t *, UBaseType_t *,
                                   UBaseType_t *waiting) {
  if (waiting) *waiting = (UBaseType_t)((Ring *)h)->data.size();
}

// ---- the library's stream write path -------------------------------------------
#define async_ws_log_e(...) ((void)0)
struct StreamUnderTest {
  std::unique_ptr<cbuf> _content;
  size_t _contentLength = 0;
  explicit StreamUnderTest(size_t bufferSize) : _content(new cbuf(bufferSize)) {}   // as the library ctor
  bool _started() const { return false; }
  size_t write(const uint8_t *data, size_t len);
};
#include "generated/stream_write.inc"   // size_t StreamUnderTest::write(...) { <library body> }

static int g_fail = 0;
#define CHECK(c, ...) do { if (!(c)) { std::printf("FAIL %s:%d  %s  ", __FILE__, __LINE__, #c); std::printf(__VA_ARGS__); std::printf("\n"); g_fail++; } \
  else { std::printf("ok   %s  ", #c); std::printf(__VA_ARGS__); std::printf("\n"); } } while (0)

static const size_t DFLT = 1460;   // RESPONSE_STREAM_BUFFER_SIZE in ESPAsyncWebServer 3.11.2

// Deterministic otmonitor-like body (the same generator as test_stream_size_hint.cpp):
// 38 entries of small prints, about 3 KB, as JsonEmit writes them.
static std::vector<size_t> otmonitorWrites(unsigned jitter) {
  std::vector<size_t> w; unsigned seed = 12345u;
  for (int field = 0; field < 38; field++) {
    const size_t parts[] = {1, 1, 14, 2, 1, 1, 7, 2, 5u + ((field + jitter) % 3), 1, 1, 6, 2, 4, 1, 1, 7, 2, 10, 1};
    for (size_t p : parts) { seed = seed * 1103515245u + 12345u; w.push_back(p + ((seed >> 16) % 2)); }
  }
  return w;
}

// One response: build the stream, write the body, read it back, destroy it.
static bool respond(size_t bufferSize, const std::vector<size_t> &w, size_t &bodyLen) {
  std::vector<uint8_t> payload; uint8_t ch = 'a';
  for (size_t n : w) for (size_t i = 0; i < n; i++) payload.push_back(ch++ % 26 + 'a');
  StreamUnderTest s(bufferSize);
  size_t off = 0;
  for (size_t n : w) { if (s.write(payload.data() + off, n) != n) return false; off += n; }
  bodyLen = s._contentLength;
  std::vector<char> back(bodyLen);
  return s._content->read(back.data(), bodyLen) == bodyLen && std::memcmp(back.data(), payload.data(), bodyLen) == 0;
}

int main() {
  const bool noPresize = std::getenv("OTGW_NO_PRESIZE") && std::getenv("OTGW_NO_PRESIZE")[0] == '1';
  const size_t otm = kRestPresize[REST_SIZE_OTMONITOR];
  const std::vector<size_t> w = otmonitorWrites(0);
  size_t len = 0;

  std::printf("== RED: the old path, a %zu B stream on the real cbuf ==\n", DFLT);
  T.reset();
  const bool okOld = respond(DFLT, w, len);
  const Tracker oldT = T;
  std::printf("     body %zu B in %zu writes: %zu ring creates (%zu distinct sizes, %zu..%zu B), %zu temp copies, peak %zu B live\n",
              len, w.size(), oldT.ringCreates, oldT.ringSizes.size(), *oldT.ringSizes.begin(), *oldT.ringSizes.rbegin(),
              oldT.tempMallocs, oldT.peak);
  CHECK(okOld, "body read back intact");
  CHECK(oldT.ringCreates > 100, "hundreds of ring buffers created for one response (%zu)", oldT.ringCreates);
  CHECK(oldT.peak > 2 * len, "transient peak above twice the body: old ring + new ring + temp copy (%zu > %zu)", oldT.peak, 2 * len);
  CHECK(oldT.live == 0, "everything freed after the response (%zu B left)", oldT.live);

  std::printf("\n== GREEN: the presize the firmware chooses (restChooseStreamSize, healthy heap) ==\n");
  const size_t chosen = noPresize ? DFLT : restChooseStreamSize(otm, 31732, DFLT);
  std::printf("     presize %zu B%s\n", chosen, noPresize ? " (OTGW_NO_PRESIZE=1: presize disabled)" : "");
  size_t creates = 0, temps = 0, peak = 0; std::set<size_t> sizes; bool okNew = true;
  for (unsigned poll = 0; poll < 50; poll++) {
    T.reset();
    okNew = respond(chosen, otmonitorWrites(poll), len) && okNew;
    creates += T.ringCreates; temps += T.tempMallocs; sizes.insert(T.ringSizes.begin(), T.ringSizes.end());
    if (T.peak > peak) peak = T.peak;
  }
  std::printf("     50 polls: %zu ring creates, %zu temp copies, %zu distinct ring size(s), peak %zu B live\n",
              creates, temps, sizes.size(), peak);
  CHECK(okNew, "every body read back intact");
  CHECK(creates == 50, "one ring buffer per response (%zu over 50 polls)", creates);
  CHECK(temps == 0, "no temporary copies (%zu)", temps);
  CHECK(sizes.size() == 1, "the same block size every poll (%zu distinct)", sizes.size());
  CHECK(peak <= chosen, "peak live equals the presize (%zu <= %zu)", peak, chosen);

  std::printf("\n== the stuck state: maxblk 7668 keeps the 1460 B start (no upfront allocation that fails) ==\n");
  CHECK(restChooseStreamSize(otm, 7668, DFLT) == DFLT, "maxblk 7668 -> %zu B", (size_t)restChooseStreamSize(otm, 7668, DFLT));

  std::printf("\n%s (%d failure(s))\n", g_fail ? "FAILED" : "ALL PASSED", g_fail);
  return g_fail ? 1 : 0;
}
