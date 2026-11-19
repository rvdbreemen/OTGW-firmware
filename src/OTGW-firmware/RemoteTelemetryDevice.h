/*
***************************************************************************
**  Program  : RemoteTelemetryDevice.h
**  (Implementation in a header: the Arduino builder generates prototypes for every .ino function before the
**  includes, which breaks on the RemoteTelemetryTls and rtel types. RemoteTelemetry.ino only includes this file.)
**  TASK-2901 proof of concept (branch research/cloud-recording only, never published): the remote telemetry recorder.
**
**  Records the OT frames, the SATTICK lines and the debug output into one mix record (RemoteTelemetryCodec.h, 4 KB),
**  closes it every 60 s or when a section is full, and sends it from its own task as an HTTPS POST to the
**  PocketBase collection point (tools/remote-telemetry/pocketbase). Certificate verification against four
**  pinned roots. Off unless /telemetry.cfg on LittleFS says enabled=1 (the opt-in):
**
**      enabled=1
**      host=rec.example.nl
**      port=443
**
**  TASK-2903: the gateway enrolls itself with its unique id (otgw-<MAC hex>) and a random secret that it keeps
**  in NVS (survives a filesystem flash), then logs in with those. Capture runs only while the service answers:
**  it starts paused, resumes after a successful login, and pauses again when the service is unreachable, with
**  the backoff of RemoteTelemetryPolicy.h before each new attempt. TASK-2904: when the maintainer blocks the gateway,
**  every request gets 403 "otgw-blocked"; the gateway then writes enabled=0 to /telemetry.cfg at once and makes
**  no further connection: the opt-in is off until the user turns it on again.
**
**  Built only in the esp32-combo-tlsdyn env (HAS_REMOTE_TELEMETRY): the stock Arduino libraries (esp-tls on mbedTLS, as
**  ESPHome uses it), with remoteTelemetryInstallTlsAllocator() putting mbedTLS blocks above 4 KB in PSRAM.
**  The remote telemetry upload runs only on a board with PSRAM (Robert, 2026-11-18: no PSRAM, no TLS).
**  A capture point waits at most 10 ms for the recorder; an item or record that is still lost is counted and
**  reported in the next record (SEC_LOST, TASK-2904), so a gap in the data is never silent.
**  Not recorded: output that bypasses the Debug* macros (e.g. the telnet banner).
***************************************************************************
*/

#pragma once

#include <Stream.h>
#include "esp_tls.h"
#include "esp_heap_caps.h"
#include "mbedtls/platform.h"
#include "esp_app_desc.h"
#include <sys/time.h>
#include <new>                        // placement new for RemoteTelemetryBufs (TASK-2901 poc.19)
#include "RemoteTelemetryCodec.h"
#include "RemoteTelemetryPolicy.h"
#include "RecRing.h"

// Roots that sign the certificates Cloudflare serves for a proxied hostname (Google Trust Services, Let's
// Encrypt). SHA-256 fingerprints checked against the system store on 2026-11-15:
//   GTS Root R1  D9:47:43:2A:BD:E7:B7:FA:90:FC:2E:6B:59:10:1B:12:80:E0:E1:C7:E4:E4:0F:A3:C6:88:7F:FF:57:A7:F4:CF
//   GTS Root R4  34:9D:FA:40:58:C5:E2:63:12:3B:39:8A:E7:95:57:3C:4E:13:13:C8:3F:E6:8F:93:55:6C:D5:E8:03:1B:3C:7D
//   ISRG Root X1 96:BC:EC:06:26:49:76:F3:74:60:77:9A:CF:28:C5:A7:CF:E8:A3:C0:AA:E1:1A:8F:FC:EE:05:C0:BD:DF:08:C6
//   ISRG Root X2 69:72:9B:8E:15:A8:6E:FC:17:7A:57:AF:B7:17:1D:FC:64:AD:D2:8C:2F:CA:8C:F1:50:7E:34:45:3C:CB:14:70
#include "RemoteTelemetryRoots.h"

// TASK-2901: mbedTLS allocates through this pair from the first line of setup(), for every user (esp-tls here,
// also the WiFi supplicant's crypto). On a board with PSRAM every block comes from PSRAM first (what ESP-IDF calls
// CONFIG_MBEDTLS_EXTERNAL_MEM_ALLOC), with internal RAM as the fallback; on a board without PSRAM the SPIRAM attempt
// fails and every block comes from internal RAM as with the stock allocator. heap_caps_free() frees a block of any
// heap, so blocks allocated before the switch are freed correctly too.
// poc.21: poc.20 still put every block up to 4 KB in internal RAM (parsed roots, the kept peer chain, the TLS
// context, ECDH/ECDSA bignums); with the 10 KB task stack that starved .63 with BLE on (09, 2026-11-19 20:18-20:29).
// Replaces poc.10's dynamic mbedTLS buffers, whose RX-buffer free panicked on .63 (core dump 2026-11-19).
static const size_t REMOTE_TELEMETRY_TLS_PSRAM_MIN = 0;
static void *remoteTelemetryTlsCalloc(size_t n, size_t size) {
  size_t total;
  if (__builtin_mul_overflow(n, size, &total)) return nullptr;
  if (total > REMOTE_TELEMETRY_TLS_PSRAM_MIN) {
    void *p = heap_caps_calloc(1, total, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (p) return p;
  }
  return heap_caps_calloc(1, total, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
}
static void remoteTelemetryTlsFree(void *p) { heap_caps_free(p); }
void remoteTelemetryInstallTlsAllocator() { mbedtls_platform_set_calloc_free(remoteTelemetryTlsCalloc, remoteTelemetryTlsFree); }

// TASK-2905: the upload connection, on ESP-IDF's esp-tls. With CONFIG_ESP_TLS_CLIENT_SESSION_TICKETS (not in
// the stock Arduino libraries) it resumes a TLS session from the ticket of the last good connection; a connect
// with it that fails is retried once with a full handshake. Without it every connection is a full handshake.
// A Stream, so the HTTP code reads and writes it as before.
#ifdef CONFIG_ESP_TLS_CLIENT_SESSION_TICKETS
static esp_tls_client_session_t *remoteTelemetrySession = nullptr;
#endif
static uint32_t remoteTelemetryResumes = 0, remoteTelemetryFullHandshakes = 0;

class RemoteTelemetryTls : public Stream {
 public:
  ~RemoteTelemetryTls() { stop(); }
  bool connect(const char *host, uint16_t port) {
    for (int attempt = 0; attempt < 2; attempt++) {
      esp_tls_cfg_t cfg = {};
      cfg.cacert_buf = (const unsigned char *)REMOTE_TELEMETRY_ROOTS;
      cfg.cacert_bytes = strlen(REMOTE_TELEMETRY_ROOTS) + 1;
      cfg.timeout_ms = 10000;
#ifdef CONFIG_ESP_TLS_CLIENT_SESSION_TICKETS
      cfg.client_session = remoteTelemetrySession;
#endif
      t_ = esp_tls_init();
      if (!t_) return false;
      if (esp_tls_conn_new_sync(host, (int)strlen(host), port, &cfg, t_) == 1) {
#ifdef CONFIG_ESP_TLS_CLIENT_SESSION_TICKETS
        if (remoteTelemetrySession) { remoteTelemetryResumes++; return true; }
#endif
        remoteTelemetryFullHandshakes++;
        return true;
      }
      esp_tls_conn_destroy(t_);
      t_ = nullptr;
#ifdef CONFIG_ESP_TLS_CLIENT_SESSION_TICKETS
      if (!remoteTelemetrySession) return false;
      esp_tls_free_client_session(remoteTelemetrySession);   // the server would not resume: once more in full
      remoteTelemetrySession = nullptr;
#else
      return false;
#endif
    }
    return false;
  }
  // keep: the exchange went well, so this connection's ticket is the one to resume next time.
  void stop(bool keep = false) {
    if (!t_) return;
#ifdef CONFIG_ESP_TLS_CLIENT_SESSION_TICKETS
    if (keep) {
      esp_tls_client_session_t *ns = esp_tls_get_client_session(t_);
      if (ns) {
        if (remoteTelemetrySession) esp_tls_free_client_session(remoteTelemetrySession);
        remoteTelemetrySession = ns;
      }
    }
#else
    (void)keep;
#endif
    esp_tls_conn_destroy(t_);
    t_ = nullptr;
    rp_ = rn_ = 0;
  }
  size_t write(uint8_t b) override { return write(&b, 1); }
  size_t write(const uint8_t *buf, size_t n) override {
    size_t off = 0;
    uint32_t start = millis();
    while (t_ && off < n) {
      ssize_t r = esp_tls_conn_write(t_, buf + off, n - off);
      if (r > 0) { off += (size_t)r; continue; }
      if ((r == ESP_TLS_ERR_SSL_WANT_READ || r == ESP_TLS_ERR_SSL_WANT_WRITE) && millis() - start < 10000) continue;
      break;
    }
    return off;
  }
  int available() override {
    if (rp_ < rn_) return (int)(rn_ - rp_);
    if (!t_) return 0;
    ssize_t a = esp_tls_get_bytes_avail(t_);
    return a > 0 ? (int)a : 0;
  }
  int read() override { return fill() ? rb_[rp_++] : -1; }
  int peek() override { return fill() ? rb_[rp_] : -1; }
  void flush() override {}

 private:
  bool fill() {
    if (rp_ < rn_) return true;
    if (!t_) return false;
    ssize_t r = esp_tls_conn_read(t_, rb_, sizeof(rb_));
    if (r <= 0) return false;
    rp_ = 0;
    rn_ = (size_t)r;
    return true;
  }
  esp_tls_t *t_ = nullptr;
  uint8_t rb_[256];
  size_t rp_ = 0, rn_ = 0;
};

static const uint32_t REMOTE_TELEMETRY_INTERVAL_MS = 60000;
static const uint32_t REMOTE_TELEMETRY_TASK_STACK  = 10240;
static const uint32_t REMOTE_TELEMETRY_LOCK_WAIT_MS = 10;   // a capture point waits this long for the recorder (TASK-2904)

struct RemoteTelemetryConfig {
  bool enabled = false;
  char host[64] = "";
  uint16_t port = 443;
};

// Service state of the upload task. Capture (remoteTelemetryActive) is on only in SVC_UP.
enum RemoteTelemetrySvc : uint8_t { SVC_PROBE, SVC_UP, SVC_BACKOFF, SVC_UID_TAKEN, SVC_OFF };
static volatile RemoteTelemetrySvc remoteTelemetrySvc = SVC_OFF;   // SVC_PROBE once remoteTelemetrySetup() finds the opt-in on
static uint8_t  remoteTelemetryBackoffStep = 0;
static uint32_t remoteTelemetryNextProbe = 0;            // millis() of the next attempt in SVC_PROBE/BACKOFF/UID_TAKEN
static char remoteTelemetryUid[18] = "";                 // otgw-<12 hex>
static char remoteTelemetrySecret[33] = "";              // 128 bits, hex, from NVS
static bool remoteTelemetryFwSynced = false;             // label/fw/board sent this boot

static RemoteTelemetryConfig remoteTelemetryCfg;
// TASK-2901 poc.19: the four large buffers (open record ~13.5 KB, send buffer ~4 KB, LZ work 3.2 KB, reply 1.5 KB) are
// one block allocated by remoteTelemetryInitCore() when the upload or local recording starts: from PSRAM when the
// board has it, else from the internal heap. As statics they held ~24 KB of internal DRAM on every boot, also with the
// feature off; poc.18 left .63 (BLE on) at ~8 KB free and hung it. Every user runs only after remoteTelemetryInitCore()
// (capture is gated on remoteTelemetryActive, the upload task is started there), so the names below stay valid.
struct RemoteTelemetryBufs {
  rtel::Recorder rec;
  uint8_t        sendBuf[rtel::REC_MAX_BYTES];
  rtel::LzWork   lz;
  char           resp[1536];                         // a login or refresh reply: the device record and the token
};
static RemoteTelemetryBufs *remoteTelemetryBufs = nullptr;
#define remoteTelemetry        (remoteTelemetryBufs->rec)
#define remoteTelemetrySendBuf (remoteTelemetryBufs->sendBuf)
#define remoteTelemetryLz      (remoteTelemetryBufs->lz)
#define remoteTelemetryResp    (remoteTelemetryBufs->resp)
// Slot 0 is the send buffer above (set in remoteTelemetryInitCore). TASK-2901 poc.22: on a PSRAM board slots 1..N-1
// are one PSRAM block (~4 KB each, ~525 KB in all): a queue that holds about 20 min of records (one per ~11 s under
// debug load) while the upload backs off, so a transient failure costs no data; it drains oldest first. Without PSRAM
// only slot 0 exists, as before (TASK-2905 had one PSRAM slot).
// poc.23: the slot table itself (128 x 32 B = 4 KB) lives in PSRAM too; until remoteTelemetryInitCore() and on a
// board without PSRAM it is the single static remoteTelemetrySlot0.
static const int REMOTE_TELEMETRY_SLOTS = 128;
static rtel::Slot remoteTelemetrySlot0 = {};
static rtel::Slot *remoteTelemetrySlots = &remoteTelemetrySlot0;
static int remoteTelemetrySlotCount = 1;
static uint64_t remoteTelemetrySeqNext = 0;
static PlatformMutex remoteTelemetryLock = nullptr;
static volatile bool remoteTelemetryActive = false;
static char remoteTelemetryToken[400] = "";        // a PocketBase JWT is about 250 characters
static char remoteTelemetryDevId[24] = "";

// Counters for the status line.
static uint32_t remoteTelemetrySent = 0, remoteTelemetryFailed = 0, remoteTelemetryDroppedItems = 0, remoteTelemetryDroppedRecords = 0;
static uint32_t remoteTelemetryLastStatus = 0, remoteTelemetryLastMs = 0;
static uint32_t remoteTelemetryHeapLow = 0xFFFFFFFFu, remoteTelemetryBlockLow = 0xFFFFFFFFu;
// TASK-2901: internal free, largest internal block and PSRAM free right after the last connect, i.e. with the TLS
// session and both record buffers allocated: shows where remoteTelemetryTlsCalloc() put them.
static uint32_t remoteTelemetryConnHeap = 0, remoteTelemetryConnBlock = 0, remoteTelemetryConnPsram = 0;

// Pending line prefix (from _debugBOL) and the tee's copy of the current line.
static bool     recBolPending = false;
static const char *recBolFn = nullptr;
static int      recBolLine = 0;
static int64_t  recBolUs = 0;
static uint32_t recHeap = 0, recBlock = 0;
static char     recTee[256];
static size_t   recTeeLen = 0;

RemoteTelemetryTee remoteTelemetryTee;

static int64_t remoteTelemetryNowUs() {
  timeval tv;
  gettimeofday(&tv, nullptr);
  return (int64_t)tv.tv_sec * 1000000LL + tv.tv_usec;
}

static inline bool remoteTelemetryInFlash(const void *p) {
  uint32_t a = (uint32_t)(uintptr_t)p;
  return a >= rtel::DROM_BASE && a < rtel::DROM_BASE + 0x1000000u;
}

// Items lost while the recorder was not available; moved into the next record's SEC_LOST under the lock.
static uint32_t remoteTelemetryLostPending = 0;
static inline void remoteTelemetryLoseItem() {
  __atomic_fetch_add(&remoteTelemetryLostPending, 1u, __ATOMIC_RELAXED);
  remoteTelemetryDroppedItems++;
}

// ---------------------------------------------------------------- record queue (caller holds remoteTelemetryLock)

static void remoteTelemetryCloseLocked(int64_t reopenMs) {
  remoteTelemetry.lostItems += __atomic_exchange_n(&remoteTelemetryLostPending, 0u, __ATOMIC_RELAXED);
  if (remoteTelemetry.open && !remoteTelemetry.empty()) {
    uint64_t seq = remoteTelemetrySeqNext++;
    rtel::Slot *f = rtel::slotFree(remoteTelemetrySlots, remoteTelemetrySlotCount);
    if (!f) {
      remoteTelemetryDroppedRecords++;                     // every slot still waits to be sent: this record is lost,
      remoteTelemetry.lostRecords++;                       // and the next record says so
    } else {
      f->t0 = remoteTelemetry.t0Ms;
      f->len = remoteTelemetry.close(f->buf, rtel::REC_MAX_BYTES);
      f->seq = seq;
      f->stored = false;
      f->busy = f->len > 0;
    }
  }
  remoteTelemetry.begin(reopenMs);
}

template <typename F> static void remoteTelemetryAppend(int64_t tMs, F f) {
  if (!remoteTelemetryActive) return;
  if (!platformMutexLock(remoteTelemetryLock, REMOTE_TELEMETRY_LOCK_WAIT_MS)) { remoteTelemetryLoseItem(); return; }
  if (!f()) {
    remoteTelemetryCloseLocked(tMs);
    if (!f()) remoteTelemetryLoseItem();
  }
  platformMutexUnlock(remoteTelemetryLock);
}

// ---------------------------------------------------------------- capture points

struct RemoteTelemetryVaSrc {
  va_list *ap;
  int64_t sint(int l) { return l >= 2 ? (int64_t)va_arg(*ap, long long) : l == 1 ? (int64_t)va_arg(*ap, long) : (int64_t)va_arg(*ap, int); }
  uint64_t uint(int l) {
    return l >= 2 ? (uint64_t)va_arg(*ap, unsigned long long) : l == 1 ? (uint64_t)va_arg(*ap, unsigned long)
                                                                        : (uint64_t)va_arg(*ap, unsigned int);
  }
  double dbl() { return va_arg(*ap, double); }
  int chr() { return va_arg(*ap, int); }
  const char *str() { return va_arg(*ap, const char *); }
};

static void remoteTelemetryTeeFlushLocked() {
  if (!recTeeLen) return;
  if (recBolPending) {
    rtel::DbgState before = remoteTelemetry.dbgSt;
    size_t mark = remoteTelemetry.dbgMark();
    rtel::dbgHeap(remoteTelemetry.dbg, remoteTelemetry.dbgSt, recHeap, recBlock);
    rtel::dbgLineRaw(remoteTelemetry.dbg, remoteTelemetry.dbgSt, recBolUs, (uint32_t)(uintptr_t)recBolFn, recBolLine, recTee, recTeeLen);
    if (!remoteTelemetry.dbgCommit(mark, before)) {
      remoteTelemetryCloseLocked(recBolUs / 1000);
      rtel::dbgHeap(remoteTelemetry.dbg, remoteTelemetry.dbgSt, recHeap, recBlock);
      rtel::dbgLineRaw(remoteTelemetry.dbg, remoteTelemetry.dbgSt, recBolUs, (uint32_t)(uintptr_t)recBolFn, recBolLine, recTee, recTeeLen);
      if (remoteTelemetry.dbg.full) { remoteTelemetry.dbg.full = false; remoteTelemetryLoseItem(); }
    }
    recBolPending = false;
  } else {
    size_t mark = remoteTelemetry.dbgMark();
    rtel::dbgRaw(remoteTelemetry.dbg, recTee, recTeeLen);
    if (remoteTelemetry.dbg.full) {
      remoteTelemetry.dbg.n = mark; remoteTelemetry.dbg.full = false;
      remoteTelemetryCloseLocked(remoteTelemetryNowUs() / 1000);
      rtel::dbgRaw(remoteTelemetry.dbg, recTee, recTeeLen);
      if (remoteTelemetry.dbg.full) { remoteTelemetry.dbg.full = false; remoteTelemetryLoseItem(); }
    }
  }
  recTeeLen = 0;
}

void remoteTelemetryTeeFlush() {
  if (!remoteTelemetryActive || !recTeeLen) return;
  if (!platformMutexLock(remoteTelemetryLock, REMOTE_TELEMETRY_LOCK_WAIT_MS)) { recTeeLen = 0; remoteTelemetryLoseItem(); return; }
  remoteTelemetryTeeFlushLocked();
  platformMutexUnlock(remoteTelemetryLock);
}

size_t RemoteTelemetryTee::write(uint8_t c) {
  debugTelnet.write(c);
  if (remoteTelemetryActive) {
    if (recTeeLen < sizeof(recTee)) recTee[recTeeLen++] = (char)c;
    if (c == '\n' || recTeeLen == sizeof(recTee)) remoteTelemetryTeeFlush();
  }
  return 1;
}

size_t RemoteTelemetryTee::write(const uint8_t *buf, size_t size) {
  for (size_t i = 0; i < size; i++) write(buf[i]);
  return size;
}

void remoteTelemetryBol(const char *fn, int line, int64_t tUs, uint32_t heap, uint32_t block) {
  if (!remoteTelemetryActive) return;
  remoteTelemetryTeeFlush();                               // a partial line before this prefix ends here
  recBolPending = true; recBolFn = fn; recBolLine = line; recBolUs = tUs; recHeap = heap; recBlock = block;
}

void remoteTelemetryPrintf(const char *fmt, va_list ap, const char *printed, size_t printedLen, bool truncated) {
  if (!remoteTelemetryActive) return;
  remoteTelemetryTeeFlush();                               // text printed after the prefix and before this printf
  if (!platformMutexLock(remoteTelemetryLock, REMOTE_TELEMETRY_LOCK_WAIT_MS)) { remoteTelemetryLoseItem(); recBolPending = false; return; }
  int64_t tUs = recBolPending ? recBolUs : remoteTelemetryNowUs();
  if (!strncmp(printed, "SATTICK ", 8)) {
    for (int fi = 0; fi < 2; fi++) {
      if (strcmp(fmt, rtel::SAT_FORMATS[fi])) continue;
      if (!remoteTelemetry.satTick(tUs / 1000, fi, printed)) {
        remoteTelemetryCloseLocked(tUs / 1000);
        if (!remoteTelemetry.satTick(tUs / 1000, fi, printed)) remoteTelemetryLoseItem();
      }
    }
  }
  if (recBolPending && remoteTelemetryInFlash(fmt) && remoteTelemetryInFlash(recBolFn)) {
    for (int attempt = 0; attempt < 2; attempt++) {
      size_t mark = remoteTelemetry.dbgMark();
      rtel::DbgState before = remoteTelemetry.dbgSt;
      rtel::dbgHeap(remoteTelemetry.dbg, remoteTelemetry.dbgSt, recHeap, recBlock);
      va_list a2;
      va_copy(a2, ap);
      RemoteTelemetryVaSrc src{&a2};
      rtel::TokResult tr = rtel::dbgPrintfTokens(remoteTelemetry, tUs, (uint32_t)(uintptr_t)fmt, (uint32_t)(uintptr_t)recBolFn,
                                                 recBolLine, fmt, src, truncated);
      va_end(a2);
      if (tr == rtel::TOK_NO) {
        rtel::dbgLineRaw(remoteTelemetry.dbg, remoteTelemetry.dbgSt, tUs, (uint32_t)(uintptr_t)recBolFn, recBolLine, printed, printedLen);
        tr = remoteTelemetry.dbgCommit(mark, before) ? rtel::TOK_OK : rtel::TOK_FULL;
      } else if (tr == rtel::TOK_FULL) {
        remoteTelemetry.dbg.n = mark; remoteTelemetry.dbgSt = before;
      }
      if (tr == rtel::TOK_OK) break;
      if (attempt == 0) remoteTelemetryCloseLocked(tUs / 1000); else remoteTelemetryLoseItem();
    }
    recBolPending = false;
  } else {
    if (recBolPending) {                            // a prefix whose function name is not a flash string
      memcpy(recTee, printed, printedLen < sizeof(recTee) ? printedLen : sizeof(recTee));
      recTeeLen = printedLen < sizeof(recTee) ? printedLen : sizeof(recTee);
    } else {
      size_t n = printedLen < sizeof(recTee) ? printedLen : sizeof(recTee);
      memcpy(recTee, printed, n);
      recTeeLen = n;
    }
    remoteTelemetryTeeFlushLocked();
  }
  platformMutexUnlock(remoteTelemetryLock);
}

void remoteTelemetrySatTickf(const char *fmt, ...) {
  if (!remoteTelemetryActive) return;
  int fi = -1;
  for (int k = 0; k < 2; k++) if (!strcmp(fmt, rtel::SAT_FORMATS[k])) fi = k;
  if (fi < 0) return;
  char text[512];
  va_list ap;
  va_start(ap, fmt);
  int n = vsnprintf(text, sizeof(text), fmt, ap);
  va_end(ap);
  if (n < 0 || n >= (int)sizeof(text)) { remoteTelemetryLoseItem(); return; }
  int64_t tMs = remoteTelemetryNowUs() / 1000;
  remoteTelemetryAppend(tMs, [&] { return remoteTelemetry.satTick(tMs, fi, text); });
}

// ---------------------------------------------------------------- TASK-1473: events (SEC_EVT)

// A longer text is cut and the cut counted as a lost item. The event is built on the caller's stack (the WiFi event
// task, async_tcp, the PIC task...), so it stays small: 256 B per text, a buffer of N bytes per event kind.
static const size_t REMOTE_TELEMETRY_EV_TEXT_MAX = 256;
static const size_t REMOTE_TELEMETRY_EV_BIG = 2 * (REMOTE_TELEMETRY_EV_TEXT_MAX + 3) + 8;   // two texts and a few fields
static const size_t REMOTE_TELEMETRY_EV_SMALL = 48;                                 // numbers only

template <size_t N = REMOTE_TELEMETRY_EV_BIG> struct RemoteTelemetryEvN {
  uint8_t m[N];
  rtel::Buf b;
  RemoteTelemetryEvN() { b.init(m, sizeof(m)); }
  void text(const char *s, size_t n) {
    if (!s) n = 0;
    if (n > REMOTE_TELEMETRY_EV_TEXT_MAX) { n = REMOTE_TELEMETRY_EV_TEXT_MAX; remoteTelemetryLoseItem(); }
    b.leb(n);
    b.bytes(s, n);
  }
  void textz(const char *s) { text(s, s ? strlen(s) : 0); }
  void u8(uint8_t v) { b.put(v); }
  void u(uint64_t v) { b.leb(v); }
  void z(int64_t v) { b.leb(rtel::zz(v)); }
  void f(float v) { uint8_t x[4]; memcpy(x, &v, 4); b.bytes(x, 4); }
  void u32le(uint32_t v) { for (int k = 0; k < 4; k++) b.put((uint8_t)(v >> (8 * k))); }
  void send(uint8_t type) {
    if (!remoteTelemetryActive) return;
    if (b.full) { remoteTelemetryLoseItem(); return; }
    int64_t tMs = remoteTelemetryNowUs() / 1000;
    remoteTelemetryAppend(tMs, [&] { return remoteTelemetry.event(tMs, type, m, b.n); });
  }
};
typedef RemoteTelemetryEvN<> RemoteTelemetryEv;
typedef RemoteTelemetryEvN<REMOTE_TELEMETRY_EV_SMALL> RemoteTelemetryEvS;

static void remoteTelemetryEvText(uint8_t type, const char *text, size_t n) {
  if (!remoteTelemetryActive) return;
  RemoteTelemetryEv e;
  e.text(text, n);
  e.send(type);
}
void remoteTelemetryEvPicCmd(const char *text, size_t n) { remoteTelemetryEvText(rtel::EV_PIC_CMD, text, n); }
void remoteTelemetryEvOtdCmd(const char *text, size_t n) { remoteTelemetryEvText(rtel::EV_OTD_CMD, text, n); }
void remoteTelemetryEvResponse(const char *text, size_t n) { remoteTelemetryEvText(rtel::EV_RESPONSE, text, n); }

void remoteTelemetryEvMqttPub(const char *topic, const char *value) {
  if (!remoteTelemetryActive) return;
  RemoteTelemetryEv e;
  e.textz(topic);
  e.textz(value);
  e.send(rtel::EV_MQTT_PUB);
}

void remoteTelemetryEvMqttIn(const char *topic, const char *value, size_t n) {
  if (!remoteTelemetryActive) return;
  char tmp[REMOTE_TELEMETRY_EV_TEXT_MAX + 1];
  size_t k = n < REMOTE_TELEMETRY_EV_TEXT_MAX ? n : REMOTE_TELEMETRY_EV_TEXT_MAX;
  if (k) memcpy(tmp, value, k);
  tmp[k] = 0;
  diagRedactBody(tmp, k);                         // the same redaction as the local recorder (ADR-213)
  if (n > k) remoteTelemetryLoseItem();
  RemoteTelemetryEv e;
  e.textz(topic);
  e.text(tmp, k);
  e.send(rtel::EV_MQTT_IN);
}

// body: nullptr or "" when the route's body must stay out (settings, secret-like route names).
void remoteTelemetryEvRestIn(uint8_t route, uint8_t method, const char *body, size_t n) {
  if (!remoteTelemetryActive) return;
  char tmp[REMOTE_TELEMETRY_EV_TEXT_MAX + 1];
  size_t k = (body && n) ? (n < REMOTE_TELEMETRY_EV_TEXT_MAX ? n : REMOTE_TELEMETRY_EV_TEXT_MAX) : 0;
  if (k) memcpy(tmp, body, k);
  tmp[k] = 0;
  diagRedactBody(tmp, k);
  if (body && n > k) remoteTelemetryLoseItem();
  RemoteTelemetryEv e;
  e.u8(route);
  e.u8(method);
  e.text(tmp, k);
  e.send(rtel::EV_REST_IN);
}

// GETs answered 200 are counted per route and sent as one EV_REST_GETS item a minute (decision 2 A, TASK-2901).
static const uint8_t REMOTE_TELEMETRY_ROUTES_MAX = 96;
static uint32_t remoteTelemetryGets[REMOTE_TELEMETRY_ROUTES_MAX];

void remoteTelemetryEvRest(uint8_t route, uint8_t method, int status) {
  if (!remoteTelemetryActive) return;
  if (method == HTTP_GET && status == 200 && route < REMOTE_TELEMETRY_ROUTES_MAX) {
    __atomic_fetch_add(&remoteTelemetryGets[route], 1u, __ATOMIC_RELAXED);
    return;
  }
  RemoteTelemetryEvS e;
  e.u8(route);
  e.u8(method);
  e.u((uint64_t)(status < 0 ? 0 : status));
  e.send(rtel::EV_REST);
}

static void remoteTelemetryFlushGets() {
  RemoteTelemetryEv e;
  uint8_t n = 0;
  size_t at = e.b.n;
  e.u8(0);                                        // count, patched below
  for (uint8_t r = 0; r < REMOTE_TELEMETRY_ROUTES_MAX; r++) {
    uint32_t c = __atomic_exchange_n(&remoteTelemetryGets[r], 0u, __ATOMIC_RELAXED);
    if (!c) continue;
    e.u8(r);
    e.u(c);
    n++;
  }
  if (!n) return;
  e.m[at] = n;
  e.send(rtel::EV_REST_GETS);
}

void remoteTelemetryEvWeather(float temperature, float humidity, float wind) {
  if (!remoteTelemetryActive) return;
  RemoteTelemetryEvS e;
  e.f(temperature); e.f(humidity); e.f(wind);
  e.send(rtel::EV_WEATHER);
}

void remoteTelemetryEvBle(uint8_t slot, float temperature, float humidity, uint8_t battery, int8_t rssi) {
  if (!remoteTelemetryActive) return;
  RemoteTelemetryEvS e;
  e.u8(slot); e.f(temperature); e.f(humidity); e.u8(battery); e.z(rssi);
  e.send(rtel::EV_BLE);
}

void remoteTelemetryEvSetting(const char *field, const char *value) {
  if (!remoteTelemetryActive) return;
  char red[REMOTE_TELEMETRY_EV_TEXT_MAX + 1];
  diagRedactSetting(field, value ? value : "", red, sizeof(red));
  RemoteTelemetryEv e;
  e.textz(field);
  e.textz(red);
  e.send(rtel::EV_SETTING);
}

void remoteTelemetryEvTimeSync(uint32_t epoch) {
  if (!remoteTelemetryActive) return;
  RemoteTelemetryEvS e;
  e.u(epoch);
  e.send(rtel::EV_TIME_SYNC);
}

void remoteTelemetryEvDallas(uint8_t index, const char *address, float tempC) {
  if (!remoteTelemetryActive) return;
  RemoteTelemetryEv e;
  e.u8(index);
  e.textz(address);
  float x = tempC * 128.0f;                       // the DS18B20 steps are 1/128 degC: exact as an integer
  e.z((int64_t)(x < 0 ? x - 0.5f : x + 0.5f));
  e.send(rtel::EV_DALLAS);
}

void remoteTelemetryEvS0(uint32_t pulses, uint32_t total, uint32_t durationMs) {
  if (!remoteTelemetryActive) return;
  RemoteTelemetryEvS e;
  e.u(pulses); e.u(total); e.u(durationMs);
  e.send(rtel::EV_S0);
}

void remoteTelemetryEvNet(uint8_t kind, int8_t rssi, uint32_t ip) {
  if (!remoteTelemetryActive) return;
  RemoteTelemetryEvS e;
  e.u8(kind); e.z(rssi); e.u32le(ip);
  e.send(rtel::EV_NET);
}

void remoteTelemetryEvMqttConn(bool up, uint8_t reason) {
  if (!remoteTelemetryActive) return;
  RemoteTelemetryEvS e;
  e.u8(up ? 1 : 0); e.u8(reason);
  e.send(rtel::EV_MQTT_CONN);
}

void remoteTelemetryEvTelnetKey(char key) {
  if (!remoteTelemetryActive) return;
  RemoteTelemetryEvS e;
  e.u8((uint8_t)key);
  e.send(rtel::EV_TELNET_KEY);
}

void remoteTelemetryEvWebhook(bool on, int status) {
  if (!remoteTelemetryActive) return;
  RemoteTelemetryEvS e;
  e.u8(on ? 1 : 0); e.z(status);
  e.send(rtel::EV_WEBHOOK);
}

void remoteTelemetryEvFw(uint8_t kind, int result, const char *text) {
  if (!remoteTelemetryActive) return;
  RemoteTelemetryEv e;
  e.u8(kind); e.z(result); e.textz(text);
  e.send(rtel::EV_FW);
}

void remoteTelemetryEvMode(uint8_t kind, uint32_t value) {
  if (!remoteTelemetryActive) return;
  RemoteTelemetryEvS e;
  e.u8(kind); e.u(value);
  e.send(rtel::EV_MODE);
}

// A command line from the port 25238 bridge (ADR decision 1 A, TASK-2901): recorded only while the collection pointd
// recorder logs; refused = dropped by the ADR-217 SAT filter (or another drop reason).
void remoteTelemetryEvBridge(const char *text, size_t n, bool refused) {
  if (!remoteTelemetryActive) return;
  RemoteTelemetryEv e;
  e.u8(refused ? 1 : 0);
  e.text(text, n);
  e.send(rtel::EV_BRIDGE_IN);
}

// Once a minute from the upload task: health, and the GET counts.
static void remoteTelemetryEvMinute() {
  if (!remoteTelemetryActive) return;
  RemoteTelemetryEvS e;
  e.u(platformFreeHeap()); e.u(platformMaxFreeBlock()); e.u(platformMinFreeHeap());
  e.z(WiFi.isConnected() ? WiFi.RSSI() : 0);
  e.u(millis() / 1000u);
  e.send(rtel::EV_HEALTH);
  remoteTelemetryFlushGets();
}

// The reboot that started this session, once, at the first resume.
static bool remoteTelemetryRebootSent = false;
static void remoteTelemetryEvBoot() {
  if (remoteTelemetryRebootSent || !remoteTelemetryActive) return;
  remoteTelemetryRebootSent = true;
  char reason[48];
  platformResetReason(reason, sizeof(reason));
  RemoteTelemetryEv e;
  e.u(state.uptime.iRebootCount);
  e.textz(reason);
  e.textz("");
  e.send(rtel::EV_REBOOT);
}

// TASK-2901: every OT frame captured gets the next sequence number, also one the recorder then cannot hold, so a
// missing frame shows on the laptop as a hole in the numbers (each record carries its first and last, SEC_FSEQ).
static uint32_t remoteTelemetryFrameSeq = 0;

// TASK-2901 (Robert 2026-11-19): a capture session starts with a marker in real time, and every local midnight (and
// every session start) gets a date marker, so the decoder can cut exact day files that run 00:00:00-24:00:00.
static int remoteTelemetryDateYday = -1, remoteTelemetryDateYear = -1;
static void remoteTelemetryEvDateIfNew(bool force) {
  if (!remoteTelemetryActive) return;
  time_t now = time(nullptr);
  struct tm lt;
  localtime_r(&now, &lt);
  if (!force && lt.tm_yday == remoteTelemetryDateYday && lt.tm_year == remoteTelemetryDateYear) return;
  remoteTelemetryDateYday = lt.tm_yday;
  remoteTelemetryDateYear = lt.tm_year;
  struct tm mid = lt;
  mid.tm_hour = mid.tm_min = mid.tm_sec = 0;
  mid.tm_isdst = -1;
  char date[11];
  snprintf_P(date, sizeof(date), PSTR("%04d-%02d-%02d"), lt.tm_year + 1900, lt.tm_mon + 1, lt.tm_mday);
  RemoteTelemetryEv e;
  e.u((uint64_t)mktime(&mid) * 1000ULL);
  e.textz(date);
  e.send(rtel::EV_DATE);
}

static void remoteTelemetryEvSession() {
  RemoteTelemetryEv e;
  e.u((uint64_t)(remoteTelemetryNowUs() / 1000));
  e.u(remoteTelemetryFrameSeq + 1);
  e.textz(_VERSION);
  e.textz(remoteTelemetryUid);
  e.send(rtel::EV_SESSION);
  remoteTelemetryEvDateIfNew(true);
}

void remoteTelemetryOtFrame(char letter, uint32_t frame) {
  if (!remoteTelemetryActive) return;
  int64_t tMs = remoteTelemetryNowUs() / 1000;
  // TASK-2901: a replayed frame carries the replay's ideal dispatch time (otFrameIdealMs, millis()): stamp it with that,
  // so the decoded download repeats the input file's gaps to the ms (Robert: output identical to the input).
  if (otFrameIdealMs) tMs -= (int64_t)(uint32_t)(millis() - otFrameIdealMs);
  const uint32_t seq = ++remoteTelemetryFrameSeq;
  remoteTelemetryAppend(tMs, [&] { return remoteTelemetry.otFrame(tMs, letter, frame, seq); });
}

// ---------------------------------------------------------------- upload

static int remoteTelemetryReadStatus(RemoteTelemetryTls &c, char *body, size_t bodyCap) {
  char line[160];
  size_t n = c.readBytesUntil('\n', line, sizeof(line) - 1);
  line[n] = 0;
  int status = 0;
  if (strncmp(line, "HTTP/1.", 7) == 0) status = atoi(line + 9);
  size_t contentLen = 0;
  for (;;) {
    n = c.readBytesUntil('\n', line, sizeof(line) - 1);
    line[n] = 0;
    if (n == 0 || (n == 1 && line[0] == '\r')) break;
    if (!strncasecmp(line, "Content-Length:", 15)) contentLen = (size_t)atol(line + 15);
  }
  size_t got = 0;
  while (got < contentLen) {
    char ch;
    if (c.readBytes(&ch, 1) != 1) break;
    if (body && got + 1 < bodyCap) body[got] = ch;
    got++;
  }
  if (body && bodyCap) body[got < bodyCap ? got : bodyCap - 1] = 0;
  return status;
}

static bool remoteTelemetryJsonString(const char *json, const char *after, const char *key, char *out, size_t cap) {
  const char *p = after ? strstr(json, after) : json;
  if (!p) return false;
  p = strstr(p, key);
  if (!p) return false;
  p += strlen(key);
  size_t i = 0;
  while (*p && *p != '"' && i + 1 < cap) out[i++] = *p++;
  out[i] = 0;
  return *p == '"';
}

static void remoteTelemetryWriteHead(RemoteTelemetryTls &c, const char *path, size_t len, bool auth) {
  char h[256];
  snprintf_P(h, sizeof(h), PSTR("POST %s HTTP/1.1\r\nHost: %s\r\nContent-Type: application/json\r\nContent-Length: %u\r\n"
                                "Connection: keep-alive\r\n"), path, remoteTelemetryCfg.host, (unsigned)len);
  c.print(h);
  if (auth) { c.print(F("Authorization: ")); c.print(remoteTelemetryToken); c.print(F("\r\n")); }
  c.print(F("\r\n"));
}

static void remoteTelemetryWriteJson(RemoteTelemetryTls &c, const char *method, const char *path, const char *body, bool auth) {
  char h[256];
  snprintf_P(h, sizeof(h), PSTR("%s %s HTTP/1.1\r\nHost: %s\r\nContent-Type: application/json\r\nContent-Length: %u\r\n"
                                "Connection: keep-alive\r\n"), method, path, remoteTelemetryCfg.host, (unsigned)strlen(body));
  c.print(h);
  if (auth) { c.print(F("Authorization: ")); c.print(remoteTelemetryToken); c.print(F("\r\n")); }
  c.print(F("\r\n"));
  c.print(body);
}

// remoteTelemetryResp: in RemoteTelemetryBufs (TASK-2901 poc.19)

// ---------------------------------------------------------------- TASK-2906: telemetry mode set centrally

static const uint32_t REMOTE_TELEMETRY_REFRESH_MS = 15UL * 60UL * 1000UL;   // token refresh: picks up a changed mode
static rtel::RemoteCfg remoteTelemetryRemote = {rtel::RM_DEFAULT, 0, 0};
static uint16_t remoteTelemetryRemoteApplied = 0;        // the log flags the current mode switches on
static uint16_t remoteTelemetryRemoteOwned = 0;          // the ones this code switched on (the user had them off)
static uint32_t remoteTelemetryLastRefresh = 0;

// Switches the log flags to what the mode asks at this moment. A flag the user had on stays on when the mode ends;
// a flag this code switched on goes off again. Never a simulation switch (an entry with an onChange action).
static void remoteTelemetryApplyRemote() {
  uint16_t want = rtel::remoteEffective(remoteTelemetryRemote, (int64_t)time(nullptr));
  if (want == remoteTelemetryRemoteApplied) return;
  for (int i = 0; i < rtel::REMOTE_FLAG_COUNT; i++) {
    const DebugFlagDef *d = debugFlagByName(rtel::REMOTE_FLAGS[i]);
    if (!d || d->onChange) continue;
    bool &f = state.debug.*(d->member);
    uint16_t bit = (uint16_t)(1u << i);
    if (want & bit) {
      if (!f) { f = true; remoteTelemetryRemoteOwned |= bit; }
    } else if (remoteTelemetryRemoteOwned & bit) {
      f = false;
      remoteTelemetryRemoteOwned &= (uint16_t)~bit;
    }
  }
  remoteTelemetryRemoteApplied = want;
  remoteTelemetryEvMode(2, ((uint32_t)remoteTelemetryRemote.mode << 16) | want);   // TASK-1473: mode << 16 | log flags
  DebugTf(PSTR("telemetry: telemetry mode %u, log flags 0x%03x%s\r\n"), (unsigned)remoteTelemetryRemote.mode, (unsigned)want,
          remoteTelemetryRemote.untilEpoch ? " (until mode_until)" : "");
}

// Reads the token and the record id from a login reply.
static bool remoteTelemetryTakeAuth() {
  if (!remoteTelemetryJsonString(remoteTelemetryResp, nullptr, "\"token\":\"", remoteTelemetryToken, sizeof(remoteTelemetryToken))) return false;
  if (!remoteTelemetryJsonString(remoteTelemetryResp, "\"record\"", "\"id\":\"", remoteTelemetryDevId, sizeof(remoteTelemetryDevId))) return false;
  char fw[sizeof(_VERSION)];
  remoteTelemetryFwSynced = remoteTelemetryFwSynced ||
                     (remoteTelemetryJsonString(remoteTelemetryResp, "\"record\"", "\"fw\":\"", fw, sizeof(fw)) && !strcmp(fw, _VERSION));
  return true;
}

// Logs in with uid + secret; enrolls the uid first when the server does not know it. Returns the HTTP status
// of the last step (200 = logged in); blocked is set when the reply says the maintainer blocked this gateway.
static int remoteTelemetryLogin(RemoteTelemetryTls &c, bool &blocked) {
  char body[200];
  blocked = false;
  snprintf_P(body, sizeof(body), PSTR("{\"identity\":\"%s\",\"password\":\"%s\"}"), remoteTelemetryUid, remoteTelemetrySecret);
  remoteTelemetryWriteJson(c, "POST", "/api/collections/devices/auth-with-password", body, false);
  int st = remoteTelemetryReadStatus(c, remoteTelemetryResp, sizeof(remoteTelemetryResp));
  if (st == 400) {                                // unknown uid (or a wrong secret): enroll
    snprintf_P(body, sizeof(body), PSTR("{\"uid\":\"%s\",\"password\":\"%s\",\"passwordConfirm\":\"%s\"}"),
               remoteTelemetryUid, remoteTelemetrySecret, remoteTelemetrySecret);
    remoteTelemetryWriteJson(c, "POST", "/api/collections/devices/records?fields=id", body, false);
    st = remoteTelemetryReadStatus(c, nullptr, 0);
    if (st == 400) return 409;                    // the uid is enrolled with another secret
    if (st != 200) return st;
    DebugTf(PSTR("telemetry: enrolled as %s\r\n"), remoteTelemetryUid);
    snprintf_P(body, sizeof(body), PSTR("{\"identity\":\"%s\",\"password\":\"%s\"}"), remoteTelemetryUid, remoteTelemetrySecret);
    remoteTelemetryWriteJson(c, "POST", "/api/collections/devices/auth-with-password", body, false);
    st = remoteTelemetryReadStatus(c, remoteTelemetryResp, sizeof(remoteTelemetryResp));
  }
  if (rtel::classifyReply(st, remoteTelemetryResp) == rtel::OUT_BLOCKED) { blocked = true; return st; }
  if (st != 200) return st;
  if (!remoteTelemetryTakeAuth()) return 0;
  remoteTelemetryRemote = rtel::parseRemote(remoteTelemetryResp);   // TASK-2906: the mode the maintainer set for this gateway
  remoteTelemetryApplyRemote();
  remoteTelemetryLastRefresh = millis();
  if (!remoteTelemetryFwSynced) {            // keep the register current: label, firmware, board
    char path[64];
    snprintf_P(path, sizeof(path), PSTR("/api/collections/devices/records/%s?fields=id"), remoteTelemetryDevId);
    snprintf_P(body, sizeof(body), PSTR("{\"label\":\"%s\",\"fw\":\"%s\",\"board\":\"%s\"}"),
               settings.sHostname, _VERSION, (const char *)hardwareTypeName());
    remoteTelemetryWriteJson(c, "PATCH", path, body, true);
    remoteTelemetryFwSynced = remoteTelemetryReadStatus(c, nullptr, 0) == 200;
  }
  return 200;
}

// Token refresh on a fresh connection (TASK-2906): a new token, and the device record with its current mode.
// Returns the HTTP status; blocked is set on a 403 otgw-blocked.
static int remoteTelemetryRefresh(bool &blocked) {
  blocked = false;
  RemoteTelemetryTls c;
  c.setTimeout(10000);
  if (!c.connect(remoteTelemetryCfg.host, remoteTelemetryCfg.port)) return 0;
  remoteTelemetryWriteJson(c, "POST", "/api/collections/devices/auth-refresh", "{}", true);
  int st = remoteTelemetryReadStatus(c, remoteTelemetryResp, sizeof(remoteTelemetryResp));
  c.stop(st > 0);
  if (rtel::classifyReply(st, remoteTelemetryResp) == rtel::OUT_BLOCKED) { blocked = true; return st; }
  if (st != 200 || !remoteTelemetryTakeAuth()) return st == 200 ? 0 : st;
  remoteTelemetryRemote = rtel::parseRemote(remoteTelemetryResp);
  remoteTelemetryApplyRemote();
  remoteTelemetryLastRefresh = millis();
  return 200;
}

// Base64 straight to the TLS client, one byte at a time; used as a rtel::Buf sink, so the LZ-coded record never
// needs its own buffer (TASK-2904).
struct RemoteTelemetryB64 {
  RemoteTelemetryTls *c;
  uint8_t q[3];
  uint8_t nq;
  size_t o;
  char out[256];
};

static void remoteTelemetryB64Emit(RemoteTelemetryB64 &s, int n) {
  static const char tab[] PROGMEM = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  uint32_t v = (uint32_t)s.q[0] << 16 | (n > 1 ? (uint32_t)s.q[1] << 8 : 0) | (n > 2 ? s.q[2] : 0);
  s.out[s.o++] = tab[(v >> 18) & 63];
  s.out[s.o++] = tab[(v >> 12) & 63];
  s.out[s.o++] = n > 1 ? tab[(v >> 6) & 63] : '=';
  s.out[s.o++] = n > 2 ? tab[v & 63] : '=';
  if (s.o + 4 > sizeof(s.out)) { s.c->write((const uint8_t *)s.out, s.o); s.o = 0; }
}

static void remoteTelemetryB64Put(void *ctx, uint8_t b) {
  RemoteTelemetryB64 &s = *(RemoteTelemetryB64 *)ctx;
  s.q[s.nq++] = b;
  if (s.nq == 3) { remoteTelemetryB64Emit(s, 3); s.nq = 0; }
}

static void remoteTelemetryB64End(RemoteTelemetryB64 &s) {
  if (s.nq) remoteTelemetryB64Emit(s, s.nq);
  if (s.o) s.c->write((const uint8_t *)s.out, s.o);
}

static void remoteTelemetryCountPut(void *, uint8_t) {}

// remoteTelemetryLz (3.2 KB, used only by the upload task): in RemoteTelemetryBufs (TASK-2901 poc.19)

// TASK-2901 poc.21: no connection attempt while the internal heap is low, so the upload can never starve the gateway
// (WiFi, web, MQTT). poc.22 limits: 12 KB free and a 4 KB largest block (WIFI_STARVED_MAXBLOCK, the firmware's own
// "starved" line). poc.21 used 16 KB / 8 KB, which blocked every upload on .63 under replay load although an upload had
// just succeeded at 14.4 KB free / 7.7 KB block (09, 2026-11-19 21:08:31). A skipped attempt is retried in 30 s
// without a backoff step: it is a local condition, not the server's.
static const uint32_t REMOTE_TELEMETRY_MIN_FREE  = 12288;
static const uint32_t REMOTE_TELEMETRY_MIN_BLOCK = 4096;
static const uint32_t REMOTE_TELEMETRY_SKIP_RETRY_MS = 30000;
static bool remoteTelemetryLastSkipped = false;
static uint32_t remoteTelemetryConnects = 0, remoteTelemetryConnectFails = 0, remoteTelemetryHeapSkips = 0;
static uint32_t remoteTelemetryFailHeap = 0, remoteTelemetryFailBlock = 0;

static bool remoteTelemetryConnect(RemoteTelemetryTls &c) {
  c.setTimeout(10000);                              // Stream read timeout, ms
  const uint32_t heap0 = platformFreeHeap(), block0 = platformMaxFreeBlock();
  if (heap0 < REMOTE_TELEMETRY_MIN_FREE || block0 < REMOTE_TELEMETRY_MIN_BLOCK) {
    remoteTelemetryHeapSkips++;
    remoteTelemetryLastSkipped = true;
    DebugTf(PSTR("telemetry: heap too low to connect (heap %u, block %u); skipped\r\n"), (unsigned)heap0, (unsigned)block0);
    return false;
  }
  remoteTelemetryConnects++;
  if (!c.connect(remoteTelemetryCfg.host, remoteTelemetryCfg.port)) {
    remoteTelemetryConnectFails++;
    remoteTelemetryFailHeap = platformFreeHeap();
    remoteTelemetryFailBlock = platformMaxFreeBlock();
    DebugTf(PSTR("telemetry: connect to %s:%u failed (heap %u, block %u)\r\n"), remoteTelemetryCfg.host,
            (unsigned)remoteTelemetryCfg.port, (unsigned)remoteTelemetryFailHeap, (unsigned)remoteTelemetryFailBlock);
    return false;
  }
  uint32_t heapTls = platformFreeHeap(), blockTls = platformMaxFreeBlock();
  remoteTelemetryConnHeap = heapTls;
  remoteTelemetryConnBlock = blockTls;
  remoteTelemetryConnPsram = (uint32_t)heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
  if (heapTls < remoteTelemetryHeapLow) remoteTelemetryHeapLow = heapTls;
  if (blockTls < remoteTelemetryBlockLow) remoteTelemetryBlockLow = blockTls;
  return true;
}

// Sends the queued record. Returns the outcome; blocked is set when the server says this gateway is blocked.
static rtel::Outcome remoteTelemetryUpload(rtel::Slot &q, bool &blocked) {
  blocked = false;
  uint32_t t = millis();
  RemoteTelemetryTls c;
  if (!remoteTelemetryConnect(c)) { remoteTelemetryLastStatus = 0; return rtel::OUT_DOWN; }
  // TASK-2904: send the record LZ-coded (fmt 5) when that is smaller, else as it is (fmt 4). The first pass only
  // counts, so Content-Length is known before anything is sent.
  rtel::Buf cnt;
  cnt.initSink(remoteTelemetryCountPut, nullptr);
  rtel::lzEncode(q.buf, q.len, cnt, remoteTelemetryLz);
  const bool lz = cnt.n < q.len;
  const size_t sendLen = lz ? cnt.n : q.len;
  // Read the full hash that esptool patches into the image. ESP-IDF's
  // formatted getter is limited by CONFIG_APP_RETRIEVE_LEN_ELF_SHA.
  const volatile uint8_t* elfSha = esp_app_get_description()->app_elf_sha256;
  const char* hex = "0123456789abcdef";
  char buildId[65];
  for (size_t i = 0; i < 32; i++) {
    const uint8_t byte = elfSha[i];
    buildId[2 * i] = hex[byte >> 4];
    buildId[2 * i + 1] = hex[byte & 0x0f];
  }
  buildId[64] = '\0';
  int st = 200;
  for (int attempt = 0; attempt < 2; attempt++) {
    if (!remoteTelemetryToken[0]) {
      st = remoteTelemetryLogin(c, blocked);
      if (st != 200 || blocked) break;
    }
    char head[240];
    int hl = snprintf_P(head, sizeof(head),
                        PSTR("{\"device\":\"%s\",\"stream\":\"mix\",\"seq\":%llu,\"t0\":%lld,\"fmt\":%d,\"build_id\":\"%s\",\"payload\":\""),
                        remoteTelemetryDevId, (unsigned long long)q.seq, (long long)q.t0, lz ? 5 : 4, buildId);
    size_t b64 = 4 * ((sendLen + 2) / 3);
    remoteTelemetryWriteHead(c, "/api/collections/batches/records?fields=id", (size_t)hl + b64 + 2, true);
    c.write((const uint8_t *)head, hl);
    RemoteTelemetryB64 s64{&c, {0, 0, 0}, 0, 0, {}};
    if (lz) {                                     // second pass: the same bytes, now to the wire
      rtel::Buf b;
      b.initSink(remoteTelemetryB64Put, &s64);
      rtel::lzEncode(q.buf, q.len, b, remoteTelemetryLz);
    } else {
      for (size_t k = 0; k < q.len; k++) remoteTelemetryB64Put(&s64, q.buf[k]);
    }
    remoteTelemetryB64End(s64);
    c.print(F("\"}"));
    st = remoteTelemetryReadStatus(c, remoteTelemetryResp, sizeof(remoteTelemetryResp));
    if (rtel::classifyReply(st, remoteTelemetryResp) == rtel::OUT_BLOCKED) { blocked = true; break; }
    if (rtel::classify(st) != rtel::OUT_AUTH) break;
    remoteTelemetryToken[0] = 0;                         // expired or revoked token: log in once more and resend
  }
  c.stop(st > 0);                                   // an HTTP answer came back: keep this session's ticket
  remoteTelemetryLastStatus = (uint32_t)st;
  remoteTelemetryLastMs = millis() - t;
  rtel::Outcome o = st == 409 ? rtel::OUT_REFUSED : rtel::classify(st);
  if (o == rtel::OUT_OK || (o == rtel::OUT_REFUSED && !blocked && st != 409)) remoteTelemetrySent++; else remoteTelemetryFailed++;
  DebugTf(PSTR("telemetry: seq %llu %u B (%u B sent) HTTP %d in %u ms; TLS resumed %u full %u, low %u/%u, min %u; conn heap %u/%u psram %u\r\n"),
          (unsigned long long)q.seq, (unsigned)q.len, (unsigned)sendLen, st, (unsigned)remoteTelemetryLastMs,
          (unsigned)remoteTelemetryResumes, (unsigned)remoteTelemetryFullHandshakes, (unsigned)remoteTelemetryHeapLow,
          (unsigned)remoteTelemetryBlockLow, (unsigned)platformMinFreeHeap(), (unsigned)remoteTelemetryConnHeap,
          (unsigned)remoteTelemetryConnBlock, (unsigned)remoteTelemetryConnPsram);
  return st == 409 ? rtel::OUT_DOWN : o;          // 409 only comes from a login; handled by the probe
}

// One login on a fresh connection: is the service there, and may this gateway log? Returns the HTTP status
// (200 logged in, 409 uid enrolled with another secret, 0 no answer).
static int remoteTelemetryProbe(bool &blocked) {
  blocked = false;
  RemoteTelemetryTls c;
  if (!remoteTelemetryConnect(c)) return 0;
  remoteTelemetryToken[0] = 0;
  int st = remoteTelemetryLogin(c, blocked);
  c.stop(st > 0);
  return st;
}

// ---------------------------------------------------------------- service state

// Stops capture. keep: the open record joins the send slot (or is counted lost when the slot is taken) and the
// queued record waits for the service; it is sent first after the next successful login. !keep (blocked): all
// of it goes.
// ---------------------------------------------------------------- TASK-2907: the local sink (one recorder, two sinks)
//
// The same closed records go to a local ring (RecRing.h) while local recording is on, and to the collection point
// while the collection point logs. Local recording is the ADR-213 switch (settings.diag.bRecordingOn, the /api/v2/diag/recording
// routes); in this env it drives this ring instead of diagRec. PSRAM boards: a 1.5 MB ring, allocated once.
// Boards without PSRAM: 8 KB of heap, refused when less than 5,120 B (ADR-030) would stay free.

static const uint32_t REMOTE_TELEMETRY_RING_PSRAM = 1536u * 1024u;
static const uint32_t REMOTE_TELEMETRY_RING_HEAP = 8192u;
static rtel::RecRing remoteTelemetryRing;
static PlatformMutex remoteTelemetryRingLock = nullptr;
static volatile bool remoteTelemetryLocalOn = false;
static uint8_t remoteTelemetryLocalLastStart = DIAG_START_OK;
static uint8_t *remoteTelemetryRingPsram = nullptr;      // allocated once, never freed
static uint8_t *remoteTelemetryRingHeap = nullptr;
static uint32_t remoteTelemetryLocalLost = 0;            // records that did not get into the ring
static bool remoteTelemetryTaskStarted = false;

static void remoteTelemetryTask(void *);
static void remoteTelemetryIdentity();

// The recorder itself: lock, first record, the second send slot on a PSRAM board, the task. Once.
static bool remoteTelemetryInitCore() {
  if (!remoteTelemetryBufs) {                              // TASK-2901 poc.19: PSRAM first, else the internal heap
    void *p = platformPsramAllocOnce(sizeof(RemoteTelemetryBufs));
    if (!p) p = heap_caps_malloc(sizeof(RemoteTelemetryBufs), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!p) {
      DebugTf(PSTR("telemetry: no memory for the %u B buffers; off\r\n"), (unsigned)sizeof(RemoteTelemetryBufs));
      return false;
    }
    remoteTelemetryBufs = new (p) RemoteTelemetryBufs();
    remoteTelemetrySlots[0].buf = remoteTelemetrySendBuf;
  }
  if (!remoteTelemetryLock) {
    remoteTelemetryLock = platformMutexCreate();
    // TASK-2901 poc.22/23: the PSRAM queue (one block for slots 1..N-1) and its slot table, both in PSRAM; without
    // PSRAM slot 0 only. Before the task starts, so nothing else touches the table while it is swapped.
    uint8_t *q = (uint8_t *)platformPsramAllocOnce((size_t)(REMOTE_TELEMETRY_SLOTS - 1) * rtel::REC_MAX_BYTES);
    rtel::Slot *t = q ? (rtel::Slot *)platformPsramAllocOnce(sizeof(rtel::Slot) * REMOTE_TELEMETRY_SLOTS) : nullptr;
    if (t) {
      memset(t, 0, sizeof(rtel::Slot) * REMOTE_TELEMETRY_SLOTS);
      t[0] = remoteTelemetrySlot0;
      for (int i = 1; i < REMOTE_TELEMETRY_SLOTS; i++) t[i].buf = q + (size_t)(i - 1) * rtel::REC_MAX_BYTES;
      remoteTelemetrySlots = t;
      remoteTelemetrySlotCount = REMOTE_TELEMETRY_SLOTS;
    }
    remoteTelemetry.begin(remoteTelemetryNowUs() / 1000);
    remoteTelemetryIdentity();
  }
  if (!remoteTelemetryTaskStarted) {
    // poc.23: the 10 KB stack in PSRAM (Robert 2026-11-19). The task writes no flash itself: the one config write
    // (blocked by the collection point) is handed to remoteTelemetryLoop() on the loop task.
    remoteTelemetryTaskStarted = platformTaskCreatePinnedPsramStack(remoteTelemetryTask, "telemetry", REMOTE_TELEMETRY_TASK_STACK, nullptr, 1);
    if (!remoteTelemetryTaskStarted) DebugTln(F("telemetry: task create failed"));
  }
  return remoteTelemetryLock && remoteTelemetryTaskStarted;
}

static const char *remoteTelemetryPauseWhy = nullptr;     // set just before a state change that pauses capture

// Capture runs while local recording is on or the collection point logs, once the clock is set (t0 and seq are epoch based).
// TASK-2901 poc.22: with the PSRAM queue it also runs before the first login and while the upload backs off, so a
// transient failure or a skipped attempt costs no data; a full queue still counts its losses (SEC_LOST in the next record).
static void remoteTelemetryUpdateActive() {
  // poc.24: ... and only while the queue has room for two more records; when the backend stays unreachable that long,
  // capture pauses (EV_PAUSE "send queue full") instead of losing records, and resumes with a session marker.
  int freeSlots = 0;
  for (int i = 0; i < remoteTelemetrySlotCount; i++) if (remoteTelemetrySlots[i].buf && !remoteTelemetrySlots[i].busy) freeSlots++;
  const bool roomy = freeSlots >= 2;
  if (!roomy && remoteTelemetrySvc != SVC_UP && !remoteTelemetryPauseWhy) remoteTelemetryPauseWhy = "send queue full, backend not reachable";
  const bool queued = remoteTelemetrySlotCount > 1 && roomy && (remoteTelemetrySvc == SVC_PROBE || remoteTelemetrySvc == SVC_BACKOFF);
  bool want = (remoteTelemetryLocalOn || remoteTelemetrySvc == SVC_UP || queued) && time(nullptr) >= 1700000000;
  if (want && !remoteTelemetryActive) {
    if (platformMutexLock(remoteTelemetryLock, 200)) {
      timeval tv;
      gettimeofday(&tv, nullptr);
      if (!remoteTelemetrySeqNext) remoteTelemetrySeqNext = (uint64_t)tv.tv_sec << 20;   // consecutive within a boot
      remoteTelemetry.begin(remoteTelemetryNowUs() / 1000);
      platformMutexUnlock(remoteTelemetryLock);
      remoteTelemetryActive = true;
      remoteTelemetryEvBoot();                           // TASK-1473: reset reason of this boot, once
      remoteTelemetryEvSession();                        // TASK-2901: session marker in real time + date marker
    }
  } else if (!want && remoteTelemetryActive) {
    // TASK-2901 (Robert 2026-11-19: signal back that the recording is paused): the last item before capture stops
    static const char *const why[] = {"probe", "up", "upload backs off", "uid enrolled with another secret", "upload off"};
    RemoteTelemetryEv e;
    e.b(1);
    e.textz(remoteTelemetryPauseWhy ? remoteTelemetryPauseWhy : why[remoteTelemetrySvc < 5 ? remoteTelemetrySvc : 4]);
    e.send(rtel::EV_PAUSE);
    remoteTelemetryPauseWhy = nullptr;
    remoteTelemetryActive = false;
  }
}

static void remoteTelemetryRingPut(void *, uint8_t b) { remoteTelemetryRing.byte(b); }

// Writes a closed record into the ring, LZ-coded when that is smaller, exactly as it would be sent.
static void remoteTelemetryStoreLocal(rtel::Slot &q) {
  rtel::Buf cnt;
  cnt.initSink(remoteTelemetryCountPut, nullptr);
  rtel::lzEncode(q.buf, q.len, cnt, remoteTelemetryLz);
  const bool lz = cnt.n < q.len;
  const size_t n = lz ? cnt.n : q.len;
  if (!platformMutexLock(remoteTelemetryRingLock, 200)) { remoteTelemetryLocalLost++; return; }
  if (!remoteTelemetryRing.begin(lz ? 5 : 4, (uint32_t)n, q.seq, q.t0, millis())) {
    remoteTelemetryLocalLost++;
  } else {
    if (lz) {
      rtel::Buf b;
      b.initSink(remoteTelemetryRingPut, nullptr);
      rtel::lzEncode(q.buf, q.len, b, remoteTelemetryLz);
    } else {
      for (size_t k = 0; k < q.len; k++) remoteTelemetryRing.byte(q.buf[k]);
    }
    remoteTelemetryRing.commit();
  }
  platformMutexUnlock(remoteTelemetryRingLock);
}

uint8_t remoteTelemetryLocalStart() {
  if (remoteTelemetryLocalOn) return DIAG_START_ALREADY;
  if (!remoteTelemetryRingLock) remoteTelemetryRingLock = platformMutexCreate();
  uint8_t *mem = nullptr;
  uint32_t cap = 0;
  if (platformHasPsram()) {
    if (!remoteTelemetryRingPsram) remoteTelemetryRingPsram = (uint8_t *)platformPsramAllocOnce(REMOTE_TELEMETRY_RING_PSRAM);
    mem = remoteTelemetryRingPsram;
    cap = REMOTE_TELEMETRY_RING_PSRAM;
    if (!mem) return remoteTelemetryLocalLastStart = DIAG_REFUSED_ALLOC;
  } else {
    const uint32_t need = REMOTE_TELEMETRY_RING_HEAP + (remoteTelemetryTaskStarted ? 0 : REMOTE_TELEMETRY_TASK_STACK);
    if (platformFreeHeap() < need + DIAG_REC_MIN_FREE_AFTER) return remoteTelemetryLocalLastStart = DIAG_REFUSED_HEAP;
    mem = (uint8_t *)malloc(REMOTE_TELEMETRY_RING_HEAP);
    cap = REMOTE_TELEMETRY_RING_HEAP;
    if (!mem) return remoteTelemetryLocalLastStart = DIAG_REFUSED_ALLOC;
    remoteTelemetryRingHeap = mem;
  }
  if (!platformMutexLock(remoteTelemetryRingLock, 200)) {
    if (remoteTelemetryRingHeap) { free(remoteTelemetryRingHeap); remoteTelemetryRingHeap = nullptr; }
    return remoteTelemetryLocalLastStart = DIAG_REFUSED_ALLOC;
  }
  remoteTelemetryRing.init(mem, cap);
  remoteTelemetryLocalLost = 0;
  platformMutexUnlock(remoteTelemetryRingLock);
  if (!remoteTelemetryInitCore()) return remoteTelemetryLocalLastStart = DIAG_REFUSED_ALLOC;
  remoteTelemetryLocalOn = true;
  DebugTf(PSTR("telemetry: local recording on, %u KB ring in %s\r\n"), (unsigned)(cap / 1024),
          remoteTelemetryRingPsram && mem == remoteTelemetryRingPsram ? "PSRAM" : "heap");
  return remoteTelemetryLocalLastStart = DIAG_START_OK;
}

bool remoteTelemetryLocalStop() {
  remoteTelemetryLocalOn = false;
  if (!remoteTelemetryRingLock || !platformMutexLock(remoteTelemetryRingLock, 200)) return false;
  uint8_t *toFree = remoteTelemetryRingHeap;
  remoteTelemetryRingHeap = nullptr;
  if (toFree) remoteTelemetryRing.init(nullptr, 0);   // a PSRAM ring keeps its records for a download after the stop
  platformMutexUnlock(remoteTelemetryRingLock);
  if (toFree) free(toFree);
  DebugTln(F("telemetry: local recording off"));
  return true;
}

void remoteTelemetryHealthRead(RemoteTelemetryHealth &h) {   // TASK-2901 poc.21, for /api/v2/health
  h.enabled = remoteTelemetrySvc != SVC_OFF;
  h.svc = (uint8_t)remoteTelemetrySvc;
  h.capturing = remoteTelemetryActive;
  h.connects = remoteTelemetryConnects;
  h.connectFails = remoteTelemetryConnectFails;
  h.heapSkips = remoteTelemetryHeapSkips;
  h.sent = remoteTelemetrySent;
  h.failed = remoteTelemetryFailed;
  h.lastStatus = remoteTelemetryLastStatus;
  h.connHeap = remoteTelemetryConnHeap;
  h.connBlock = remoteTelemetryConnBlock;
  h.connPsram = remoteTelemetryConnPsram;
  h.failHeap = remoteTelemetryFailHeap;
  h.failBlock = remoteTelemetryFailBlock;
}

void remoteTelemetryLocalStatusRead(RemoteTelemetryLocalStatus &st) {
  st.active = remoteTelemetryLocalOn;
  st.lastStart = remoteTelemetryLocalLastStart;
  st.psram = platformHasPsram();
  st.exact = remoteTelemetryRingLock && platformMutexTryLock(remoteTelemetryRingLock);
  st.entries = remoteTelemetryRing.entries;
  st.capBytes = remoteTelemetryRing.cap;
  st.dropped = remoteTelemetryRing.evicted + remoteTelemetryLocalLost;
  uint64_t o = 0, nw = 0;
  st.hasData = remoteTelemetryRing.ready() && remoteTelemetryRing.oldestSeq(o) && remoteTelemetryRing.newestSeq(nw);
  st.oldest = o;
  st.newest = nw;
  if (st.exact) platformMutexUnlock(remoteTelemetryRingLock);
}

// One chunk of the download (async_tcp): never waits for the lock. DIAG_FILL_TRY_AGAIN when busy, 0 at the end.
size_t remoteTelemetryLocalFill(rtel::RingCursor &c, uint8_t *buf, size_t cap) {
  if (!remoteTelemetryRingLock) return 0;
  if (!platformMutexTryLock(remoteTelemetryRingLock)) return DIAG_FILL_TRY_AGAIN;
  size_t n = remoteTelemetryRing.ready() ? rtel::ringFill(remoteTelemetryRing, c, buf, cap, millis()) : 0;
  if (!n) rtel::ringFillDone(remoteTelemetryRing);
  platformMutexUnlock(remoteTelemetryRingLock);
  return n;
}

const char *remoteTelemetryUidText() { return remoteTelemetryUid; }

// ---------------------------------------------------------------- service state

// Stops the upload side's capture (local recording keeps capturing). keep: the open record joins the send queue;
// !keep (blocked): the queue goes, unless local recording still needs it.
static void remoteTelemetryPause(bool keep) {
  if (platformMutexLock(remoteTelemetryLock, 200)) {
    if (keep || remoteTelemetryLocalOn) {
      remoteTelemetryCloseLocked(remoteTelemetryNowUs() / 1000);
    } else {
      for (int i = 0; i < remoteTelemetrySlotCount; i++) remoteTelemetrySlots[i].busy = false;
      remoteTelemetry.begin(remoteTelemetryNowUs() / 1000);
    }
    platformMutexUnlock(remoteTelemetryLock);
  }
}

static void remoteTelemetryResume() {
  remoteTelemetryBackoffStep = 0;
  remoteTelemetrySvc = SVC_UP;
  remoteTelemetryUpdateActive();
  DebugTf(PSTR("telemetry: logging as %s\r\n"), remoteTelemetryUid);
}

static void remoteTelemetryBackoff(const __FlashStringHelper *why, bool maxWait) {
  remoteTelemetryPause(true);
  const bool skipped = remoteTelemetryLastSkipped && !maxWait;   // poc.22: low local heap, not a server failure
  remoteTelemetryLastSkipped = false;
  uint32_t wait = maxWait ? rtel::BACKOFF_MAX_MS
                : skipped ? REMOTE_TELEMETRY_SKIP_RETRY_MS
                          : rtel::backoffMs(remoteTelemetryBackoffStep, platformHardwareRandom());
  if (!maxWait && !skipped && remoteTelemetryBackoffStep < 255) remoteTelemetryBackoffStep++;
  remoteTelemetryNextProbe = millis() + wait;
  remoteTelemetrySvc = maxWait ? SVC_UID_TAKEN : SVC_BACKOFF;
  remoteTelemetryUpdateActive();
  DebugTf(PSTR("telemetry: %s; %s, next attempt in %u s\r\n"), skipped ? "heap low" : (const char *)why,
          remoteTelemetryActive ? "capture goes on (queued)" : "capture paused", (unsigned)(wait / 1000));
}

static volatile bool remoteTelemetryCfgWritePending = false;
static void remoteTelemetryWriteConfig() {
  File f = LittleFS.open("/telemetry.cfg", "w");
  if (!f) return;
  f.printf_P(PSTR("enabled=%d\nhost=%s\nport=%u\n"), remoteTelemetryCfg.enabled ? 1 : 0, remoteTelemetryCfg.host, (unsigned)remoteTelemetryCfg.port);
  f.close();
}

// TASK-2901 (Robert 2026-11-19): the collection point can never turn remote telemetry off. "blocked" is only learnt from
// the reply to a delivery attempt, and it pauses: capture stops (EV_PAUSE "blocked by the collection point"), the next
// attempt follows after the longest backoff, and /telemetry.cfg stays as the user set it on the device.
static void remoteTelemetryBlocked() {
  remoteTelemetryRemote = {rtel::RM_DEFAULT, 0, 0};       // any central telemetry mode ends with it
  remoteTelemetryApplyRemote();
  remoteTelemetryPauseWhy = "blocked by the collection point";
  remoteTelemetryPause(true);
  remoteTelemetryNextProbe = millis() + rtel::BACKOFF_MAX_MS;
  remoteTelemetrySvc = SVC_UID_TAKEN;                     // the longest wait, no capture; local recording, if on, goes on
  remoteTelemetryUpdateActive();
  remoteTelemetryPauseWhy = nullptr;
  DebugTf(PSTR("telemetry: %s is blocked by the collection point; paused, next attempt in %u min

"), remoteTelemetryUid,
          (unsigned)(rtel::BACKOFF_MAX_MS / 60000));
}

// From loop(): flash writes the telemetry task may not do itself (its stack is in PSRAM, poc.23).
void remoteTelemetryLoop() {
  if (!remoteTelemetryCfgWritePending) return;
  remoteTelemetryCfgWritePending = false;
  remoteTelemetryWriteConfig();
}

static void remoteTelemetryTask(void *) {
  uint32_t lastClose = millis();
  for (;;) {
    platformTaskDelay(500);
    remoteTelemetryUpdateActive();                       // local recording on, or the clock just became valid
    remoteTelemetryEvDateIfNew(false);                   // TASK-2901: a date marker right after every local midnight
    if (remoteTelemetryActive && millis() - lastClose >= REMOTE_TELEMETRY_INTERVAL_MS) {
      lastClose = millis();
      remoteTelemetryEvMinute();                         // TASK-1473: health and GET counts, then the record closes
      if (platformMutexLock(remoteTelemetryLock, 200)) {
        remoteTelemetryCloseLocked(remoteTelemetryNowUs() / 1000);
        platformMutexUnlock(remoteTelemetryLock);
      }
    }
    // Every closed record: first the local ring, then the collection point.
    rtel::Slot *q = rtel::slotOldest(remoteTelemetrySlots, remoteTelemetrySlotCount);
    if (q) {
      if (remoteTelemetryLocalOn && !q->stored) { remoteTelemetryStoreLocal(*q); q->stored = true; }
      if (remoteTelemetrySvc == SVC_UP && isNetworkUp()) {
        bool blocked;
        rtel::Outcome o = remoteTelemetryUpload(*q, blocked);
        if (blocked) remoteTelemetryBlocked();
        else if (o == rtel::OUT_DOWN || o == rtel::OUT_AUTH) {
          remoteTelemetryBackoff(F("upload failed"), false);
          if (remoteTelemetryLocalOn) q->busy = false;   // it is in the ring; the collection point gets the records from here on
        } else {
          q->busy = false;                        // stored, or a resend of a stored seq
        }
      } else if (remoteTelemetryLocalOn || remoteTelemetrySvc == SVC_OFF) {
        q->busy = false;                          // local only, or no upload at all
      }
    }
    if (remoteTelemetrySvc == SVC_OFF) continue;         // no upload: nothing to connect
    if (!isNetworkUp()) {                         // own network down: pause, no backoff step
      if (remoteTelemetrySvc == SVC_UP) { remoteTelemetryPause(true); remoteTelemetrySvc = SVC_PROBE; remoteTelemetryNextProbe = millis(); remoteTelemetryUpdateActive(); }
      continue;
    }
    if (remoteTelemetrySvc != SVC_UP) {
      if ((int32_t)(millis() - remoteTelemetryNextProbe) < 0 || time(nullptr) < 1700000000) continue;   // and wait for NTP
      if (!remoteTelemetrySecret[0]) continue;
      bool blocked;
      int st = remoteTelemetryProbe(blocked);
      if (blocked) remoteTelemetryBlocked();
      else if (st == 200) { remoteTelemetryResume(); }
      else if (st == 409) remoteTelemetryBackoff(F("uid enrolled with another secret (delete it on the server)"), true);
      else remoteTelemetryBackoff(F("service unreachable"), false);
      continue;
    }
    remoteTelemetryApplyRemote();                        // a mode past its mode_until ends here, within a second
    if (millis() - remoteTelemetryLastRefresh >= REMOTE_TELEMETRY_REFRESH_MS) {
      bool blocked;
      int st = remoteTelemetryRefresh(blocked);
      if (blocked) { remoteTelemetryBlocked(); continue; }
      if (rtel::classify(st) == rtel::OUT_AUTH) remoteTelemetryToken[0] = 0;          // log in again at the next upload
      else if (rtel::classify(st) == rtel::OUT_DOWN) { remoteTelemetryBackoff(F("token refresh failed"), false); continue; }
      remoteTelemetryLastRefresh = millis();
    }
  }
}

// ---------------------------------------------------------------- setup

static void remoteTelemetryReadConfig() {
  File f = LittleFS.open("/telemetry.cfg", "r");
  if (!f) return;
  char line[160];
  while (f.available()) {
    size_t n = f.readBytesUntil('\n', line, sizeof(line) - 1);
    line[n] = 0;
    if (n && line[n - 1] == '\r') line[n - 1] = 0;
    char *eq = strchr(line, '=');
    if (!eq) continue;
    *eq = 0;
    const char *v = eq + 1;
    if (!strcmp_P(line, PSTR("enabled"))) remoteTelemetryCfg.enabled = atoi(v) == 1;
    else if (!strcmp_P(line, PSTR("host"))) strlcpy(remoteTelemetryCfg.host, v, sizeof(remoteTelemetryCfg.host));
    else if (!strcmp_P(line, PSTR("port"))) remoteTelemetryCfg.port = (uint16_t)atoi(v);
  }
  f.close();
}

// The gateway's identity: uid from the factory MAC, and a secret that is made once and kept in NVS, so a
// filesystem flash does not cost the gateway its enrollment.
static void remoteTelemetryIdentity() {
  uint8_t mac[6];
  platformGetMacAddress(mac);
  snprintf_P(remoteTelemetryUid, sizeof(remoteTelemetryUid), PSTR("otgw-%02x%02x%02x%02x%02x%02x"), mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  Preferences prefs;
  if (!prefs.begin("telemetry", false)) return;
  // TASK-2901: getString() returns nvs_get_str's length, which counts the NUL (33 for the 32-character secret).
  // The old check against 32 failed on every boot, so each boot replaced the enrolled secret (409 on .63).
  size_t got = prefs.getString("secret", remoteTelemetrySecret, sizeof(remoteTelemetrySecret));
  if (got != sizeof(remoteTelemetrySecret) || strlen(remoteTelemetrySecret) != sizeof(remoteTelemetrySecret) - 1) {
    for (size_t i = 0; i < sizeof(remoteTelemetrySecret) - 1; i += 8)
      snprintf_P(remoteTelemetrySecret + i, 9, PSTR("%08lx"), (unsigned long)platformHardwareRandom());
    prefs.putString("secret", remoteTelemetrySecret);
  }
  prefs.end();
}

void remoteTelemetrySetup() {
  remoteTelemetryReadConfig();
  if (!remoteTelemetryCfg.enabled || !remoteTelemetryCfg.host[0]) {
    DebugTln(F("telemetry: remote telemetry upload off (no /telemetry.cfg with enabled=1); local recording works as before"));
    return;
  }
  if (!platformHasPsram()) {
    DebugTln(F("telemetry: remote telemetry upload needs a board with PSRAM; off"));
    return;
  }
  if (!remoteTelemetryInitCore()) return;
  remoteTelemetrySvc = SVC_PROBE;                          // the upload starts after the first login says it may
  remoteTelemetryNextProbe = millis();
  DebugTf(PSTR("telemetry: on, %s:%u as %s, %d send slot(s), heap %u, block %u\r\n"), remoteTelemetryCfg.host,
          (unsigned)remoteTelemetryCfg.port, remoteTelemetryUid, remoteTelemetrySlotCount, (unsigned)platformFreeHeap(),
          (unsigned)platformMaxFreeBlock());
}
