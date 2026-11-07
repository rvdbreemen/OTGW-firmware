//=======================================================================
// test_diag_recording_routes.cpp
//
// TASK-1666 host harness, driven by test_diag_recording_routes.py. It includes the REAL
// src/OTGW-firmware/diagRecorderStuff.h and calls the real download code the REST glue in
// restAPI.ino uses: diagDownloadRange() (fixes the block range at request start),
// diagDownloadHeaderBegin()/End() (file header), diagDownloadFill() (the filler callback body)
// and diagStatusSnapshot() (status route). Only the environment is doubled (clock, heap, lock).
//
// Cases (CASE <id> pass|FAIL):
//   R1  range fixed at request start: blocks opened while the download runs are not sent
//   R2  since= skips older blocks; since at or past the newest gives an empty range;
//       since below the oldest starts at the oldest
//   R3  never started, started without data and stopped recorder: valid header-only file, count 0
//   R4  file header fields, firmware strings and the route name list round-trip; an oversize
//       header is refused
//   R5  recorder lock busy: the range is refused, the filler answers TRY_AGAIN and writes nothing
//   R6  a block overwritten after the range was fixed is served as a lost block (seq kept,
//       first_record_seq 0xFFFFFFFF, rest zero)
//   R7  status snapshot: state fields, ring size, oldest/newest, drops, refusal reason
//   R8  the body is the same bytes for chunk sizes 1, 97, 1460 and 5000
//=======================================================================

#include <cstdio>
#include <cstring>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <vector>

#include "diagRecorderStuff.h"

static int g_fail = 0;
static void caseResult(const char* id, bool ok, const std::string& why = "") {
  std::printf("CASE %s %s%s%s\n", id, ok ? "pass" : "FAIL", ok || why.empty() ? "" : "  -- ", ok ? "" : why.c_str());
  if (!ok) g_fail++;
}

// ---- environment doubles ---------------------------------------------------
static uint32_t g_ms = 1000, g_free = 60000;
static bool g_lockBusy = false;
static uint32_t eMillis() { return g_ms; }
static uint32_t eEpoch() { return 0; }
static uint32_t eFree() { return g_free; }
static void* eAllocHeap(size_t n) { return std::calloc(1, n); }
static void eFreeHeap(void* p) { std::free(p); }
static void* ePsram(size_t n) { return std::calloc(1, n); }
static bool eTry() { return !g_lockBusy; }
static bool eWait(uint32_t) { return true; }
static void eUnlock() {}

static DiagEnv makeEnv(bool psram) {
  DiagEnv e;
  e.millis = eMillis; e.epoch = eEpoch; e.freeHeap = eFree; e.allocHeap = eAllocHeap; e.freeHeapBlock = eFreeHeap;
  e.allocPsramOnce = ePsram; e.tryLock = eTry; e.lockWait = eWait; e.unlock = eUnlock; e.hasPsram = psram;
  return e;
}

static uint32_t g_ev = 0;
static void addEvents(DiagRecorder& r, uint32_t untilNewest) {      // distinct frames: the blocks fill at a steady rate
  uint32_t guard = 0;
  while ((!r.hasData() || r.newestSeq() < untilNewest) && guard++ < 5000000) { g_ms += 7; r.recOtFrame('T', 0x40000000u + (g_ev++ * 2654435761u)); }
}

static uint32_t rd32(const uint8_t* p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24); }
static uint16_t rd16(const uint8_t* p) { return (uint16_t)(p[0] | (p[1] << 8)); }

// Header of the way the glue builds it: fixed part, two route names.
static bool buildHeader(DiagRecorder& r, DiagDownload& d, const std::vector<std::string>& routes) {
  DiagHdrWriter w(d);
  diagDownloadHeaderBegin(w, d, r, 7u, r.memIsPsram, "2.0.0-test+abc1234", "abc1234");
  w.u8((uint8_t)routes.size());
  for (const auto& s : routes) w.str(s.c_str());
  return diagDownloadHeaderEnd(w, d);
}

// Read the whole body through the filler like the web server does. TRY_AGAIN retries the same index.
static std::vector<uint8_t> readAll(DiagRecorder& r, const DiagDownload& d, size_t chunk, bool* ended = nullptr, size_t mid = 0, void (*midHook)() = nullptr) {
  std::vector<uint8_t> out; std::vector<uint8_t> buf(chunk);
  size_t guard = 0; bool hooked = false;
  if (ended) *ended = false;
  while (guard++ < 40000000) {
    if (!hooked && midHook && out.size() >= mid) { midHook(); hooked = true; }
    size_t n = diagDownloadFill(r, d, buf.data(), chunk, out.size());
    if (n == DIAG_FILL_TRY_AGAIN) continue;
    if (n == 0) { if (ended) *ended = true; break; }
    out.insert(out.end(), buf.begin(), buf.begin() + n);
  }
  return out;
}

static DiagRecorder* g_hookRec = nullptr;
static void hookAppendBlocks() { addEvents(*g_hookRec, g_hookRec->newestSeq() + 4); }

int main() {
  DiagEnv ps = makeEnv(true);
  DiagEnv hp = makeEnv(false);
  const std::vector<std::string> routes = { "health", "settings", "diag" };

  // R1 ---------------------------------------------------------------------
  {
    g_ev = 0; g_free = 60000; g_lockBusy = false;
    DiagRecorder* r = new DiagRecorder();
    r->start(ps); addEvents(*r, 4);
    DiagDownload* d = new DiagDownload();
    bool rangeOk = diagDownloadRange(*r, *d, false, 0);
    const uint32_t newestAtStart = r->newestSeq();
    buildHeader(*r, *d, routes);
    const size_t expect = d->totalBytes();
    g_hookRec = r;
    bool ended = false;
    std::vector<uint8_t> body = readAll(*r, *d, 1460, &ended, d->hdrLen + 2 * DIAG_REC_BLOCK_SIZE + 100, hookAppendBlocks);
    const bool grew = r->newestSeq() >= newestAtStart + 4;      // the recorder really did open more blocks mid-download
    bool ok = rangeOk && ended && grew && d->first == 0 && d->count == newestAtStart + 1u && body.size() == expect;
    std::string why = "size=" + std::to_string(body.size()) + " expect=" + std::to_string(expect) + " grew=" + std::to_string(grew);
    for (uint32_t i = 0; ok && i < d->count; i++) {
      const uint8_t* b = body.data() + d->hdrLen + (size_t)i * DIAG_REC_BLOCK_SIZE;
      if (rd32(b) != i) { ok = false; why = "block " + std::to_string(i) + " has seq " + std::to_string(rd32(b)); }
    }
    caseResult("R1", ok, why);
    std::free(r->mem); delete r; delete d;
  }

  // R2 ---------------------------------------------------------------------
  {
    g_ev = 0; g_lockBusy = false;
    DiagRecorder* r = new DiagRecorder();
    r->start(ps); addEvents(*r, 4);
    const uint32_t newest = r->newestSeq();
    DiagDownload d1, d2, d3, d4;
    bool a = diagDownloadRange(*r, d1, true, 1);
    bool b = diagDownloadRange(*r, d2, true, newest);
    bool c = diagDownloadRange(*r, d3, true, 100000);
    bool e = diagDownloadRange(*r, d4, false, 0);
    bool ok = a && b && c && e && d1.first == 2 && d1.count == newest - 1u && d2.count == 0 && d3.count == 0 && d4.first == 0 && d4.count == newest + 1u;
    std::string why = "since=1 first=" + std::to_string(d1.first) + " count=" + std::to_string(d1.count) + " newest=" + std::to_string(newest);
    // the bytes follow the range: first block served for since=1 is block 2
    buildHeader(*r, d1, routes);
    std::vector<uint8_t> body = readAll(*r, d1, 1460);
    ok = ok && body.size() == d1.totalBytes() && rd32(body.data() + d1.hdrLen) == 2;
    std::free(r->mem); delete r;
    // since below the oldest: heap ring of one block
    DiagRecorder* h = new DiagRecorder();
    g_free = 60000; h->start(hp); addEvents(*h, 7);
    DiagDownload d5;
    bool f = diagDownloadRange(*h, d5, true, 2);
    ok = ok && f && h->oldestSeq() == h->newestSeq() && d5.first == h->oldestSeq() && d5.count == 1;
    if (!(f && d5.count == 1)) why += " heap: first=" + std::to_string(d5.first) + " count=" + std::to_string(d5.count);
    h->stop(); delete h;
    caseResult("R2", ok, why);
  }

  // R3 ---------------------------------------------------------------------
  {
    g_ev = 0; g_lockBusy = false;
    bool ok = true; std::string why;
    DiagRecorder never;                                          // never started: env == nullptr
    DiagDownload d0; ok = ok && diagDownloadRange(never, d0, false, 0) && d0.count == 0 && buildHeader(never, d0, routes);
    std::vector<uint8_t> b0 = readAll(never, d0, 1460);
    ok = ok && b0.size() == d0.hdrLen && b0.size() == d0.totalBytes() && memcmp(b0.data(), "OTGWREC", 7) == 0 && rd32(b0.data() + 20) == 0;
    if (!ok) why = "never started";
    DiagRecorder started;                                        // started, no record yet
    started.start(ps);
    DiagDownload d1; ok = ok && diagDownloadRange(started, d1, false, 0) && d1.count == 0 && buildHeader(started, d1, routes);
    ok = ok && readAll(started, d1, 1460).size() == d1.hdrLen;
    addEvents(started, 2);
    started.stop();                                              // stopped: ring gone
    DiagDownload d2; ok = ok && diagDownloadRange(started, d2, false, 0) && d2.count == 0 && buildHeader(started, d2, routes);
    std::vector<uint8_t> b2 = readAll(started, d2, 1460);
    ok = ok && b2.size() == d2.hdrLen && rd32(b2.data() + 20) == 0;
    if (ok == false && why.empty()) why = "started/stopped";
    caseResult("R3", ok, why);
    std::free(started.mem);
  }

  // R4 ---------------------------------------------------------------------
  {
    g_ev = 0; g_lockBusy = false;
    DiagRecorder* r = new DiagRecorder();
    r->start(ps); addEvents(*r, 2);
    DiagDownload d; diagDownloadRange(*r, d, false, 0);
    bool built = buildHeader(*r, d, routes);
    const uint8_t* h = d.hdr;
    bool ok = built && memcmp(h, "OTGWREC\0", 8) == 0 && rd16(h + 8) == DIAG_REC_FORMAT_VERSION && rd16(h + 10) == d.hdrLen &&
              rd32(h + 12) == DIAG_REC_BLOCK_SIZE && rd32(h + 16) == 0 && rd32(h + 20) == d.count && d.count == r->newestSeq() + 1u &&
              rd32(h + 24) == DIAG_REC_PSRAM_BLOCKS && rd32(h + 28) == 7u && (h[32] & 1) == 1;
    size_t p = 36; std::vector<std::string> got;
    auto rdStr = [&](void) { size_t l = h[p++]; std::string s((const char*)h + p, l); p += l; return s; };
    std::string ver = rdStr(), hash = rdStr();
    size_t nr = h[p++];
    for (size_t i = 0; i < nr; i++) got.push_back(rdStr());
    ok = ok && ver == "2.0.0-test+abc1234" && hash == "abc1234" && got == routes && p == d.hdrLen;
    // oversize: four 255-byte names do not fit DIAG_REC_HDR_MAX
    DiagDownload big; diagDownloadRange(*r, big, false, 0);
    std::vector<std::string> huge(4, std::string(255, 'x'));
    bool refused = !buildHeader(*r, big, huge) && big.hdrLen == 0;
    caseResult("R4", ok && refused, std::string("fixed/strings/routes ") + (ok ? "ok" : "BAD") + ", oversize " + (refused ? "refused" : "ACCEPTED"));
    std::free(r->mem); delete r;
  }

  // R5 ---------------------------------------------------------------------
  {
    g_ev = 0; g_lockBusy = false;
    DiagRecorder* r = new DiagRecorder();
    r->start(ps); addEvents(*r, 2);
    DiagDownload d; diagDownloadRange(*r, d, false, 0); buildHeader(*r, d, routes);
    g_lockBusy = true;
    DiagDownload dBusy; bool rangeBusy = diagDownloadRange(*r, dBusy, false, 0);
    uint8_t buf[64]; memset(buf, 0xA5, sizeof buf);
    size_t inBlock = diagDownloadFill(*r, d, buf, sizeof buf, d.hdrLen + 10);
    bool untouched = true; for (uint8_t v : buf) if (v != 0xA5) untouched = false;
    size_t inHdr = diagDownloadFill(*r, d, buf, sizeof buf, 0);       // the header needs no lock
    g_lockBusy = false;
    size_t after = diagDownloadFill(*r, d, buf, sizeof buf, d.hdrLen + 10);
    bool ok = !rangeBusy && inBlock == DIAG_FILL_TRY_AGAIN && untouched && inHdr == sizeof buf && after == sizeof buf;
    caseResult("R5", ok, "rangeBusy=" + std::to_string(rangeBusy) + " fillBusy=" + std::to_string(inBlock == DIAG_FILL_TRY_AGAIN) + " after=" + std::to_string(after));
    std::free(r->mem); delete r;
  }

  // R6 ---------------------------------------------------------------------
  {
    g_ev = 0; g_lockBusy = false; g_free = 60000;
    DiagRecorder* h = new DiagRecorder();
    h->start(hp); addEvents(*h, 2);                                  // one-block ring: only block 2 is held
    DiagDownload d; diagDownloadRange(*h, d, false, 0); buildHeader(*h, d, routes);
    const uint32_t served = d.first;
    addEvents(*h, served + 1);                                       // block `served` is overwritten after the range was fixed
    std::vector<uint8_t> body = readAll(*h, d, 1460);
    bool ok = d.count == 1 && body.size() == d.totalBytes();
    if (ok) {
      const uint8_t* b = body.data() + d.hdrLen;
      ok = rd32(b) == served && rd32(b + 12) == DIAG_REC_LOST_SEQ;
      for (size_t i = 4; ok && i < DIAG_REC_BLOCK_SIZE; i++) if (i < 12 || i >= 16) { if (b[i] != 0) ok = false; }
    }
    caseResult("R6", ok, "served=" + std::to_string(served) + " newest=" + std::to_string(h->newestSeq()));
    h->stop(); delete h;
  }

  // R7 ---------------------------------------------------------------------
  {
    g_ev = 0; g_lockBusy = false; g_free = 60000;
    DiagRecorder* r = new DiagRecorder();
    r->start(ps);
    DiagStatus s0; diagStatusSnapshot(*r, s0);
    bool ok = s0.active && s0.exact && s0.psram && !s0.hasData && s0.ringBlocks == DIAG_REC_PSRAM_BLOCKS &&
              s0.ringBytes == DIAG_REC_PSRAM_BLOCKS * DIAG_REC_BLOCK_SIZE && s0.oldest == 0 && s0.newest == 0 && s0.lastStart == DIAG_START_OK;
    addEvents(*r, 3);
    DiagStatus s1; diagStatusSnapshot(*r, s1);
    ok = ok && s1.hasData && s1.oldest == 0 && s1.newest == r->newestSeq() && s1.newest >= 3;
    g_lockBusy = true;
    r->recOtFrame('T', 1); r->recOtFrame('T', 2);                    // dropped: the lock is busy
    DiagStatus s2; diagStatusSnapshot(*r, s2);
    g_lockBusy = false;
    ok = ok && !s2.exact && s2.dropped >= 2;
    r->stop();
    DiagStatus s3; diagStatusSnapshot(*r, s3);
    ok = ok && !s3.active && !s3.hasData && s3.ringBytes == 0;
    std::free(r->mem); delete r;
    DiagRecorder* h = new DiagRecorder();
    g_free = 6000;                                                   // below ring + caches + 5120 B
    DiagStartResult st = h->start(hp);
    DiagStatus s4; diagStatusSnapshot(*h, s4);
    ok = ok && st == DIAG_REFUSED_HEAP && !s4.active && s4.lastStart == DIAG_REFUSED_HEAP && diagStartReasonText(s4.lastStart)[0] != '\0';
    delete h; g_free = 60000;
    DiagRecorder fresh; DiagStatus s5; diagStatusSnapshot(fresh, s5);
    ok = ok && s5.exact && !s5.active && s5.ringBytes == 0;
    caseResult("R7", ok);
  }

  // R8 ---------------------------------------------------------------------
  {
    g_ev = 0; g_lockBusy = false;
    DiagRecorder* r = new DiagRecorder();
    r->start(ps); addEvents(*r, 1);
    DiagDownload d; diagDownloadRange(*r, d, false, 0); buildHeader(*r, d, routes);
    std::vector<uint8_t> a = readAll(*r, d, 1460), b = readAll(*r, d, 1), c = readAll(*r, d, 97), e = readAll(*r, d, 5000);
    bool ok = a.size() == d.totalBytes() && a == b && a == c && a == e;
    caseResult("R8", ok, "sizes " + std::to_string(a.size()) + "/" + std::to_string(b.size()) + "/" + std::to_string(c.size()) + "/" + std::to_string(e.size()));
    std::free(r->mem); delete r;
  }

  std::printf("%s\n", g_fail ? "SOME CASES FAILED" : "ALL CASES PASSED");
  return g_fail ? 1 : 0;
}
