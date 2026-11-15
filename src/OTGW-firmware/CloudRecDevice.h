/*
***************************************************************************
**  Program  : CloudRecDevice.h
**  (Implementation in a header: the Arduino builder generates prototypes for every .ino function before the
**  includes, which breaks on the CloudRecTls and crec types. CloudRec.ino only includes this file.)
**  TASK-1963 proof of concept (branch research/cloud-recording only, never published): the cloud recorder.
**
**  Records the OT frames, the SATTICK lines and the debug output into one mix record (CloudRecCodec.h, 4 KB),
**  closes it every 60 s or when a section is full, and sends it from its own task as an HTTPS POST to the
**  PocketBase collection point (tools/cloud-recorder/pocketbase). Certificate verification against four
**  pinned roots. Off unless /cloudrec.cfg on LittleFS says enabled=1 (the opt-in):
**
**      enabled=1
**      host=rec.example.nl
**      port=443
**
**  TASK-1971: the gateway enrolls itself with its unique id (otgw-<MAC hex>) and a random secret that it keeps
**  in NVS (survives a filesystem flash), then logs in with those. Capture runs only while the service answers:
**  it starts paused, resumes after a successful login, and pauses again when the service is unreachable, with
**  the backoff of CloudRecPolicy.h before each new attempt. TASK-1972: when the maintainer blocks the gateway,
**  every request gets 403 "otgw-blocked"; the gateway then writes enabled=0 to /cloudrec.cfg at once and makes
**  no further connection: the opt-in is off until the user turns it on again.
**
**  Built only in the esp32-combo-tlsdyn env (HAS_CLOUD_REC), whose IDF libraries use dynamic mbedTLS buffers.
**  A capture point waits at most 10 ms for the recorder; an item or record that is still lost is counted and
**  reported in the next record (SEC_LOST, TASK-1972), so a gap in the data is never silent.
**  Not recorded: output that bypasses the Debug* macros (e.g. the telnet banner).
***************************************************************************
*/

#pragma once

#include <Stream.h>
#include "esp_tls.h"
#include <sys/time.h>
#include "CloudRecCodec.h"
#include "CloudRecPolicy.h"

// Roots that sign the certificates Cloudflare serves for a proxied hostname (Google Trust Services, Let's
// Encrypt). SHA-256 fingerprints checked against the system store on 2026-11-15:
//   GTS Root R1  D9:47:43:2A:BD:E7:B7:FA:90:FC:2E:6B:59:10:1B:12:80:E0:E1:C7:E4:E4:0F:A3:C6:88:7F:FF:57:A7:F4:CF
//   GTS Root R4  34:9D:FA:40:58:C5:E2:63:12:3B:39:8A:E7:95:57:3C:4E:13:13:C8:3F:E6:8F:93:55:6C:D5:E8:03:1B:3C:7D
//   ISRG Root X1 96:BC:EC:06:26:49:76:F3:74:60:77:9A:CF:28:C5:A7:CF:E8:A3:C0:AA:E1:1A:8F:FC:EE:05:C0:BD:DF:08:C6
//   ISRG Root X2 69:72:9B:8E:15:A8:6E:FC:17:7A:57:AF:B7:17:1D:FC:64:AD:D2:8C:2F:CA:8C:F1:50:7E:34:45:3C:CB:14:70
#include "CloudRecRoots.h"

// TASK-1974: the upload connection, on ESP-IDF's esp-tls instead of CloudRecTls, because esp-tls can
// resume a TLS session from a ticket (CONFIG_ESP_TLS_CLIENT_SESSION_TICKETS in the tlsdyn env): a resumed
// handshake skips the certificate chain and the key exchange's heavy part. The ticket of the last good connection
// is kept; a connect with it that fails is retried once with a full handshake. A Stream, so the HTTP code reads
// and writes it as before.
static esp_tls_client_session_t *cloudRecSession = nullptr;
static uint32_t cloudRecResumes = 0, cloudRecFullHandshakes = 0;

class CloudRecTls : public Stream {
 public:
  ~CloudRecTls() { stop(); }
  bool connect(const char *host, uint16_t port) {
    for (int attempt = 0; attempt < 2; attempt++) {
      esp_tls_cfg_t cfg = {};
      cfg.cacert_buf = (const unsigned char *)CLOUDREC_ROOTS;
      cfg.cacert_bytes = strlen(CLOUDREC_ROOTS) + 1;
      cfg.timeout_ms = 10000;
      cfg.client_session = cloudRecSession;
      t_ = esp_tls_init();
      if (!t_) return false;
      if (esp_tls_conn_new_sync(host, (int)strlen(host), port, &cfg, t_) == 1) {
        if (cloudRecSession) cloudRecResumes++; else cloudRecFullHandshakes++;
        return true;
      }
      esp_tls_conn_destroy(t_);
      t_ = nullptr;
      if (!cloudRecSession) return false;
      esp_tls_free_client_session(cloudRecSession);   // the server would not resume: once more in full
      cloudRecSession = nullptr;
    }
    return false;
  }
  // keep: the exchange went well, so this connection's ticket is the one to resume next time.
  void stop(bool keep = false) {
    if (!t_) return;
    if (keep) {
      esp_tls_client_session_t *ns = esp_tls_get_client_session(t_);
      if (ns) {
        if (cloudRecSession) esp_tls_free_client_session(cloudRecSession);
        cloudRecSession = ns;
      }
    }
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

static const uint32_t CLOUDREC_INTERVAL_MS = 60000;
static const uint32_t CLOUDREC_TASK_STACK  = 10240;
static const uint32_t CLOUDREC_LOCK_WAIT_MS = 10;   // a capture point waits this long for the recorder (TASK-1972)

struct CloudRecConfig {
  bool enabled = false;
  char host[64] = "";
  uint16_t port = 443;
};

// Service state of the upload task. Capture (cloudRecActive) is on only in SVC_UP.
enum CloudRecSvc : uint8_t { SVC_PROBE, SVC_UP, SVC_BACKOFF, SVC_UID_TAKEN, SVC_OFF };
static volatile CloudRecSvc cloudRecSvc = SVC_PROBE;
static uint8_t  cloudRecBackoffStep = 0;
static uint32_t cloudRecNextProbe = 0;            // millis() of the next attempt in SVC_PROBE/BACKOFF/UID_TAKEN
static char cloudRecUid[18] = "";                 // otgw-<12 hex>
static char cloudRecSecret[33] = "";              // 128 bits, hex, from NVS
static bool cloudRecFwSynced = false;             // label/fw/board sent this boot

static CloudRecConfig cloudRecCfg;
static crec::Recorder cloudRec;                     // the open record (about 13.5 KB, static: ADR-053)
static uint8_t  cloudRecSendBuf[crec::REC_MAX_BYTES];
// Slot 0 is the static buffer; slot 1 exists only on a PSRAM board (allocated in cloudRecSetup, TASK-1974).
static crec::Slot cloudRecSlots[2] = {{cloudRecSendBuf, 0, 0, 0, false}, {nullptr, 0, 0, 0, false}};
static uint64_t cloudRecSeqNext = 0;
static PlatformMutex cloudRecLock = nullptr;
static volatile bool cloudRecActive = false;
static char cloudRecToken[400] = "";        // a PocketBase JWT is about 250 characters
static char cloudRecDevId[24] = "";

// Counters for the status line.
static uint32_t cloudRecSent = 0, cloudRecFailed = 0, cloudRecDroppedItems = 0, cloudRecDroppedRecords = 0;
static uint32_t cloudRecLastStatus = 0, cloudRecLastMs = 0;
static uint32_t cloudRecHeapLow = 0xFFFFFFFFu, cloudRecBlockLow = 0xFFFFFFFFu;

// Pending line prefix (from _debugBOL) and the tee's copy of the current line.
static bool     recBolPending = false;
static const char *recBolFn = nullptr;
static int      recBolLine = 0;
static int64_t  recBolUs = 0;
static uint32_t recHeap = 0, recBlock = 0;
static char     recTee[256];
static size_t   recTeeLen = 0;

CloudRecTee cloudRecTee;

static int64_t cloudRecNowUs() {
  timeval tv;
  gettimeofday(&tv, nullptr);
  return (int64_t)tv.tv_sec * 1000000LL + tv.tv_usec;
}

static inline bool cloudRecInFlash(const void *p) {
  uint32_t a = (uint32_t)(uintptr_t)p;
  return a >= crec::DROM_BASE && a < crec::DROM_BASE + 0x1000000u;
}

// Items lost while the recorder was not available; moved into the next record's SEC_LOST under the lock.
static uint32_t cloudRecLostPending = 0;
static inline void cloudRecLoseItem() {
  __atomic_fetch_add(&cloudRecLostPending, 1u, __ATOMIC_RELAXED);
  cloudRecDroppedItems++;
}

// ---------------------------------------------------------------- record queue (caller holds cloudRecLock)

static void cloudRecCloseLocked(int64_t reopenMs) {
  cloudRec.lostItems += __atomic_exchange_n(&cloudRecLostPending, 0u, __ATOMIC_RELAXED);
  if (cloudRec.open && !cloudRec.empty()) {
    uint64_t seq = cloudRecSeqNext++;
    crec::Slot *f = crec::slotFree(cloudRecSlots, 2);
    if (!f) {
      cloudRecDroppedRecords++;                     // every slot still waits to be sent: this record is lost,
      cloudRec.lostRecords++;                       // and the next record says so
    } else {
      f->t0 = cloudRec.t0Ms;
      f->len = cloudRec.close(f->buf, crec::REC_MAX_BYTES);
      f->seq = seq;
      f->busy = f->len > 0;
    }
  }
  cloudRec.begin(reopenMs);
}

template <typename F> static void cloudRecAppend(int64_t tMs, F f) {
  if (!cloudRecActive) return;
  if (!platformMutexLock(cloudRecLock, CLOUDREC_LOCK_WAIT_MS)) { cloudRecLoseItem(); return; }
  if (!f()) {
    cloudRecCloseLocked(tMs);
    if (!f()) cloudRecLoseItem();
  }
  platformMutexUnlock(cloudRecLock);
}

// ---------------------------------------------------------------- capture points

struct CloudRecVaSrc {
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

static void cloudRecTeeFlushLocked() {
  if (!recTeeLen) return;
  if (recBolPending) {
    crec::DbgState before = cloudRec.dbgSt;
    size_t mark = cloudRec.dbgMark();
    crec::dbgHeap(cloudRec.dbg, cloudRec.dbgSt, recHeap, recBlock);
    crec::dbgLineRaw(cloudRec.dbg, cloudRec.dbgSt, recBolUs, (uint32_t)(uintptr_t)recBolFn, recBolLine, recTee, recTeeLen);
    if (!cloudRec.dbgCommit(mark, before)) {
      cloudRecCloseLocked(recBolUs / 1000);
      crec::dbgHeap(cloudRec.dbg, cloudRec.dbgSt, recHeap, recBlock);
      crec::dbgLineRaw(cloudRec.dbg, cloudRec.dbgSt, recBolUs, (uint32_t)(uintptr_t)recBolFn, recBolLine, recTee, recTeeLen);
      if (cloudRec.dbg.full) { cloudRec.dbg.full = false; cloudRecLoseItem(); }
    }
    recBolPending = false;
  } else {
    size_t mark = cloudRec.dbgMark();
    crec::dbgRaw(cloudRec.dbg, recTee, recTeeLen);
    if (cloudRec.dbg.full) {
      cloudRec.dbg.n = mark; cloudRec.dbg.full = false;
      cloudRecCloseLocked(cloudRecNowUs() / 1000);
      crec::dbgRaw(cloudRec.dbg, recTee, recTeeLen);
      if (cloudRec.dbg.full) { cloudRec.dbg.full = false; cloudRecLoseItem(); }
    }
  }
  recTeeLen = 0;
}

void cloudRecTeeFlush() {
  if (!cloudRecActive || !recTeeLen) return;
  if (!platformMutexLock(cloudRecLock, CLOUDREC_LOCK_WAIT_MS)) { recTeeLen = 0; cloudRecLoseItem(); return; }
  cloudRecTeeFlushLocked();
  platformMutexUnlock(cloudRecLock);
}

size_t CloudRecTee::write(uint8_t c) {
  debugTelnet.write(c);
  if (cloudRecActive) {
    if (recTeeLen < sizeof(recTee)) recTee[recTeeLen++] = (char)c;
    if (c == '\n' || recTeeLen == sizeof(recTee)) cloudRecTeeFlush();
  }
  return 1;
}

size_t CloudRecTee::write(const uint8_t *buf, size_t size) {
  for (size_t i = 0; i < size; i++) write(buf[i]);
  return size;
}

void cloudRecBol(const char *fn, int line, int64_t tUs, uint32_t heap, uint32_t block) {
  if (!cloudRecActive) return;
  cloudRecTeeFlush();                               // a partial line before this prefix ends here
  recBolPending = true; recBolFn = fn; recBolLine = line; recBolUs = tUs; recHeap = heap; recBlock = block;
}

void cloudRecPrintf(const char *fmt, va_list ap, const char *printed, size_t printedLen, bool truncated) {
  if (!cloudRecActive) return;
  cloudRecTeeFlush();                               // text printed after the prefix and before this printf
  if (!platformMutexLock(cloudRecLock, CLOUDREC_LOCK_WAIT_MS)) { cloudRecLoseItem(); recBolPending = false; return; }
  int64_t tUs = recBolPending ? recBolUs : cloudRecNowUs();
  if (!strncmp(printed, "SATTICK ", 8)) {
    for (int fi = 0; fi < 2; fi++) {
      if (strcmp(fmt, crec::SAT_FORMATS[fi])) continue;
      if (!cloudRec.satTick(tUs / 1000, fi, printed)) {
        cloudRecCloseLocked(tUs / 1000);
        if (!cloudRec.satTick(tUs / 1000, fi, printed)) cloudRecLoseItem();
      }
    }
  }
  if (recBolPending && cloudRecInFlash(fmt) && cloudRecInFlash(recBolFn)) {
    for (int attempt = 0; attempt < 2; attempt++) {
      size_t mark = cloudRec.dbgMark();
      crec::DbgState before = cloudRec.dbgSt;
      crec::dbgHeap(cloudRec.dbg, cloudRec.dbgSt, recHeap, recBlock);
      va_list a2;
      va_copy(a2, ap);
      CloudRecVaSrc src{&a2};
      crec::TokResult tr = crec::dbgPrintfTokens(cloudRec, tUs, (uint32_t)(uintptr_t)fmt, (uint32_t)(uintptr_t)recBolFn,
                                                 recBolLine, fmt, src, truncated);
      va_end(a2);
      if (tr == crec::TOK_NO) {
        crec::dbgLineRaw(cloudRec.dbg, cloudRec.dbgSt, tUs, (uint32_t)(uintptr_t)recBolFn, recBolLine, printed, printedLen);
        tr = cloudRec.dbgCommit(mark, before) ? crec::TOK_OK : crec::TOK_FULL;
      } else if (tr == crec::TOK_FULL) {
        cloudRec.dbg.n = mark; cloudRec.dbgSt = before;
      }
      if (tr == crec::TOK_OK) break;
      if (attempt == 0) cloudRecCloseLocked(tUs / 1000); else cloudRecLoseItem();
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
    cloudRecTeeFlushLocked();
  }
  platformMutexUnlock(cloudRecLock);
}

void cloudRecSatTickf(const char *fmt, ...) {
  if (!cloudRecActive) return;
  int fi = -1;
  for (int k = 0; k < 2; k++) if (!strcmp(fmt, crec::SAT_FORMATS[k])) fi = k;
  if (fi < 0) return;
  char text[512];
  va_list ap;
  va_start(ap, fmt);
  int n = vsnprintf(text, sizeof(text), fmt, ap);
  va_end(ap);
  if (n < 0 || n >= (int)sizeof(text)) { cloudRecLoseItem(); return; }
  int64_t tMs = cloudRecNowUs() / 1000;
  cloudRecAppend(tMs, [&] { return cloudRec.satTick(tMs, fi, text); });
}

void cloudRecOtFrame(char letter, uint32_t frame) {
  int64_t tMs = cloudRecNowUs() / 1000;
  cloudRecAppend(tMs, [&] { return cloudRec.otFrame(tMs, letter, frame); });
}

// ---------------------------------------------------------------- upload

static int cloudRecReadStatus(CloudRecTls &c, char *body, size_t bodyCap) {
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

static bool cloudRecJsonString(const char *json, const char *after, const char *key, char *out, size_t cap) {
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

static void cloudRecWriteHead(CloudRecTls &c, const char *path, size_t len, bool auth) {
  char h[256];
  snprintf_P(h, sizeof(h), PSTR("POST %s HTTP/1.1\r\nHost: %s\r\nContent-Type: application/json\r\nContent-Length: %u\r\n"
                                "Connection: keep-alive\r\n"), path, cloudRecCfg.host, (unsigned)len);
  c.print(h);
  if (auth) { c.print(F("Authorization: ")); c.print(cloudRecToken); c.print(F("\r\n")); }
  c.print(F("\r\n"));
}

static void cloudRecWriteJson(CloudRecTls &c, const char *method, const char *path, const char *body, bool auth) {
  char h[256];
  snprintf_P(h, sizeof(h), PSTR("%s %s HTTP/1.1\r\nHost: %s\r\nContent-Type: application/json\r\nContent-Length: %u\r\n"
                                "Connection: keep-alive\r\n"), method, path, cloudRecCfg.host, (unsigned)strlen(body));
  c.print(h);
  if (auth) { c.print(F("Authorization: ")); c.print(cloudRecToken); c.print(F("\r\n")); }
  c.print(F("\r\n"));
  c.print(body);
}

static char cloudRecResp[1536];                  // a login or refresh reply: the device record and the token

// ---------------------------------------------------------------- TASK-1979: telemetry mode set centrally

static const uint32_t CLOUDREC_REFRESH_MS = 15UL * 60UL * 1000UL;   // token refresh: picks up a changed mode
static crec::RemoteCfg cloudRecRemote = {crec::RM_DEFAULT, 0, 0};
static uint16_t cloudRecRemoteApplied = 0;        // the log flags the current mode switches on
static uint16_t cloudRecRemoteOwned = 0;          // the ones this code switched on (the user had them off)
static uint32_t cloudRecLastRefresh = 0;

// Switches the log flags to what the mode asks at this moment. A flag the user had on stays on when the mode ends;
// a flag this code switched on goes off again. Never a simulation switch (an entry with an onChange action).
static void cloudRecApplyRemote() {
  uint16_t want = crec::remoteEffective(cloudRecRemote, (int64_t)time(nullptr));
  if (want == cloudRecRemoteApplied) return;
  for (int i = 0; i < crec::REMOTE_FLAG_COUNT; i++) {
    const DebugFlagDef *d = debugFlagByName(crec::REMOTE_FLAGS[i]);
    if (!d || d->onChange) continue;
    bool &f = state.debug.*(d->member);
    uint16_t bit = (uint16_t)(1u << i);
    if (want & bit) {
      if (!f) { f = true; cloudRecRemoteOwned |= bit; }
    } else if (cloudRecRemoteOwned & bit) {
      f = false;
      cloudRecRemoteOwned &= (uint16_t)~bit;
    }
  }
  cloudRecRemoteApplied = want;
  DebugTf(PSTR("cloudrec: telemetry mode %u, log flags 0x%03x%s\r\n"), (unsigned)cloudRecRemote.mode, (unsigned)want,
          cloudRecRemote.untilEpoch ? " (until mode_until)" : "");
}

// Reads the token and the record id from a login reply.
static bool cloudRecTakeAuth() {
  if (!cloudRecJsonString(cloudRecResp, nullptr, "\"token\":\"", cloudRecToken, sizeof(cloudRecToken))) return false;
  if (!cloudRecJsonString(cloudRecResp, "\"record\"", "\"id\":\"", cloudRecDevId, sizeof(cloudRecDevId))) return false;
  char fw[sizeof(_VERSION)];
  cloudRecFwSynced = cloudRecFwSynced ||
                     (cloudRecJsonString(cloudRecResp, "\"record\"", "\"fw\":\"", fw, sizeof(fw)) && !strcmp(fw, _VERSION));
  return true;
}

// Logs in with uid + secret; enrolls the uid first when the server does not know it. Returns the HTTP status
// of the last step (200 = logged in); blocked is set when the reply says the maintainer blocked this gateway.
static int cloudRecLogin(CloudRecTls &c, bool &blocked) {
  char body[200];
  blocked = false;
  snprintf_P(body, sizeof(body), PSTR("{\"identity\":\"%s\",\"password\":\"%s\"}"), cloudRecUid, cloudRecSecret);
  cloudRecWriteJson(c, "POST", "/api/collections/devices/auth-with-password", body, false);
  int st = cloudRecReadStatus(c, cloudRecResp, sizeof(cloudRecResp));
  if (st == 400) {                                // unknown uid (or a wrong secret): enroll
    snprintf_P(body, sizeof(body), PSTR("{\"uid\":\"%s\",\"password\":\"%s\",\"passwordConfirm\":\"%s\"}"),
               cloudRecUid, cloudRecSecret, cloudRecSecret);
    cloudRecWriteJson(c, "POST", "/api/collections/devices/records?fields=id", body, false);
    st = cloudRecReadStatus(c, nullptr, 0);
    if (st == 400) return 409;                    // the uid is enrolled with another secret
    if (st != 200) return st;
    DebugTf(PSTR("cloudrec: enrolled as %s\r\n"), cloudRecUid);
    snprintf_P(body, sizeof(body), PSTR("{\"identity\":\"%s\",\"password\":\"%s\"}"), cloudRecUid, cloudRecSecret);
    cloudRecWriteJson(c, "POST", "/api/collections/devices/auth-with-password", body, false);
    st = cloudRecReadStatus(c, cloudRecResp, sizeof(cloudRecResp));
  }
  if (crec::classifyReply(st, cloudRecResp) == crec::OUT_BLOCKED) { blocked = true; return st; }
  if (st != 200) return st;
  if (!cloudRecTakeAuth()) return 0;
  cloudRecRemote = crec::parseRemote(cloudRecResp);   // TASK-1979: the mode the maintainer set for this gateway
  cloudRecApplyRemote();
  cloudRecLastRefresh = millis();
  if (!cloudRecFwSynced) {            // keep the register current: label, firmware, board
    char path[64];
    snprintf_P(path, sizeof(path), PSTR("/api/collections/devices/records/%s?fields=id"), cloudRecDevId);
    snprintf_P(body, sizeof(body), PSTR("{\"label\":\"%s\",\"fw\":\"%s\",\"board\":\"%s\"}"),
               settings.sHostname, _VERSION, (const char *)hardwareTypeName());
    cloudRecWriteJson(c, "PATCH", path, body, true);
    cloudRecFwSynced = cloudRecReadStatus(c, nullptr, 0) == 200;
  }
  return 200;
}

// Token refresh on a fresh connection (TASK-1979): a new token, and the device record with its current mode.
// Returns the HTTP status; blocked is set on a 403 otgw-blocked.
static int cloudRecRefresh(bool &blocked) {
  blocked = false;
  CloudRecTls c;
  c.setTimeout(10000);
  if (!c.connect(cloudRecCfg.host, cloudRecCfg.port)) return 0;
  cloudRecWriteJson(c, "POST", "/api/collections/devices/auth-refresh", "{}", true);
  int st = cloudRecReadStatus(c, cloudRecResp, sizeof(cloudRecResp));
  c.stop(st > 0);
  if (crec::classifyReply(st, cloudRecResp) == crec::OUT_BLOCKED) { blocked = true; return st; }
  if (st != 200 || !cloudRecTakeAuth()) return st == 200 ? 0 : st;
  cloudRecRemote = crec::parseRemote(cloudRecResp);
  cloudRecApplyRemote();
  cloudRecLastRefresh = millis();
  return 200;
}

// Base64 straight to the TLS client, one byte at a time; used as a crec::Buf sink, so the LZ-coded record never
// needs its own buffer (TASK-1972).
struct CloudRecB64 {
  CloudRecTls *c;
  uint8_t q[3];
  uint8_t nq;
  size_t o;
  char out[256];
};

static void cloudRecB64Emit(CloudRecB64 &s, int n) {
  static const char tab[] PROGMEM = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  uint32_t v = (uint32_t)s.q[0] << 16 | (n > 1 ? (uint32_t)s.q[1] << 8 : 0) | (n > 2 ? s.q[2] : 0);
  s.out[s.o++] = tab[(v >> 18) & 63];
  s.out[s.o++] = tab[(v >> 12) & 63];
  s.out[s.o++] = n > 1 ? tab[(v >> 6) & 63] : '=';
  s.out[s.o++] = n > 2 ? tab[v & 63] : '=';
  if (s.o + 4 > sizeof(s.out)) { s.c->write((const uint8_t *)s.out, s.o); s.o = 0; }
}

static void cloudRecB64Put(void *ctx, uint8_t b) {
  CloudRecB64 &s = *(CloudRecB64 *)ctx;
  s.q[s.nq++] = b;
  if (s.nq == 3) { cloudRecB64Emit(s, 3); s.nq = 0; }
}

static void cloudRecB64End(CloudRecB64 &s) {
  if (s.nq) cloudRecB64Emit(s, s.nq);
  if (s.o) s.c->write((const uint8_t *)s.out, s.o);
}

static void cloudRecCountPut(void *, uint8_t) {}

static crec::LzWork cloudRecLz;                    // 3.2 KB, used only by the upload task

static bool cloudRecConnect(CloudRecTls &c) {
  c.setTimeout(10000);                              // Stream read timeout, ms
  if (!c.connect(cloudRecCfg.host, cloudRecCfg.port)) {
    DebugTf(PSTR("cloudrec: connect to %s:%u failed (heap %u, block %u)\r\n"), cloudRecCfg.host,
            (unsigned)cloudRecCfg.port, (unsigned)platformFreeHeap(), (unsigned)platformMaxFreeBlock());
    return false;
  }
  uint32_t heapTls = platformFreeHeap(), blockTls = platformMaxFreeBlock();
  if (heapTls < cloudRecHeapLow) cloudRecHeapLow = heapTls;
  if (blockTls < cloudRecBlockLow) cloudRecBlockLow = blockTls;
  return true;
}

// Sends the queued record. Returns the outcome; blocked is set when the server says this gateway is blocked.
static crec::Outcome cloudRecUpload(crec::Slot &q, bool &blocked) {
  blocked = false;
  uint32_t t = millis();
  CloudRecTls c;
  if (!cloudRecConnect(c)) { cloudRecLastStatus = 0; return crec::OUT_DOWN; }
  // TASK-1972: send the record LZ-coded (fmt 5) when that is smaller, else as it is (fmt 4). The first pass only
  // counts, so Content-Length is known before anything is sent.
  crec::Buf cnt;
  cnt.initSink(cloudRecCountPut, nullptr);
  crec::lzEncode(q.buf, q.len, cnt, cloudRecLz);
  const bool lz = cnt.n < q.len;
  const size_t sendLen = lz ? cnt.n : q.len;
  int st = 200;
  for (int attempt = 0; attempt < 2; attempt++) {
    if (!cloudRecToken[0]) {
      st = cloudRecLogin(c, blocked);
      if (st != 200 || blocked) break;
    }
    char head[200];
    int hl = snprintf_P(head, sizeof(head), PSTR("{\"device\":\"%s\",\"stream\":\"mix\",\"seq\":%llu,\"t0\":%lld,\"fmt\":%d,\"payload\":\""),
                        cloudRecDevId, (unsigned long long)q.seq, (long long)q.t0, lz ? 5 : 4);
    size_t b64 = 4 * ((sendLen + 2) / 3);
    cloudRecWriteHead(c, "/api/collections/batches/records?fields=id", (size_t)hl + b64 + 2, true);
    c.write((const uint8_t *)head, hl);
    CloudRecB64 s64{&c, {0, 0, 0}, 0, 0, {}};
    if (lz) {                                     // second pass: the same bytes, now to the wire
      crec::Buf b;
      b.initSink(cloudRecB64Put, &s64);
      crec::lzEncode(q.buf, q.len, b, cloudRecLz);
    } else {
      for (size_t k = 0; k < q.len; k++) cloudRecB64Put(&s64, q.buf[k]);
    }
    cloudRecB64End(s64);
    c.print(F("\"}"));
    st = cloudRecReadStatus(c, cloudRecResp, sizeof(cloudRecResp));
    if (crec::classifyReply(st, cloudRecResp) == crec::OUT_BLOCKED) { blocked = true; break; }
    if (crec::classify(st) != crec::OUT_AUTH) break;
    cloudRecToken[0] = 0;                         // expired or revoked token: log in once more and resend
  }
  c.stop(st > 0);                                   // an HTTP answer came back: keep this session's ticket
  cloudRecLastStatus = (uint32_t)st;
  cloudRecLastMs = millis() - t;
  crec::Outcome o = st == 409 ? crec::OUT_REFUSED : crec::classify(st);
  if (o == crec::OUT_OK || (o == crec::OUT_REFUSED && !blocked && st != 409)) cloudRecSent++; else cloudRecFailed++;
  DebugTf(PSTR("cloudrec: seq %llu %u B (%u B sent) HTTP %d in %u ms; TLS resumed %u full %u, low %u/%u, min %u\r\n"),
          (unsigned long long)q.seq, (unsigned)q.len, (unsigned)sendLen, st, (unsigned)cloudRecLastMs,
          (unsigned)cloudRecResumes, (unsigned)cloudRecFullHandshakes, (unsigned)cloudRecHeapLow,
          (unsigned)cloudRecBlockLow, (unsigned)platformMinFreeHeap());
  return st == 409 ? crec::OUT_DOWN : o;          // 409 only comes from a login; handled by the probe
}

// One login on a fresh connection: is the service there, and may this gateway log? Returns the HTTP status
// (200 logged in, 409 uid enrolled with another secret, 0 no answer).
static int cloudRecProbe(bool &blocked) {
  blocked = false;
  CloudRecTls c;
  if (!cloudRecConnect(c)) return 0;
  cloudRecToken[0] = 0;
  int st = cloudRecLogin(c, blocked);
  c.stop(st > 0);
  return st;
}

// ---------------------------------------------------------------- service state

// Stops capture. keep: the open record joins the send slot (or is counted lost when the slot is taken) and the
// queued record waits for the service; it is sent first after the next successful login. !keep (blocked): all
// of it goes.
static void cloudRecPause(bool keep) {
  cloudRecActive = false;
  if (platformMutexLock(cloudRecLock, 200)) {
    if (keep) {
      cloudRecCloseLocked(cloudRecNowUs() / 1000);
    } else {
      for (auto &q : cloudRecSlots) q.busy = false;
      cloudRec.begin(cloudRecNowUs() / 1000);
    }
    platformMutexUnlock(cloudRecLock);
  }
}

static void cloudRecResume() {
  if (platformMutexLock(cloudRecLock, 200)) {
    timeval tv;
    gettimeofday(&tv, nullptr);
    if (!cloudRecSeqNext) cloudRecSeqNext = (uint64_t)tv.tv_sec << 20;   // consecutive within a boot
    cloudRec.begin(cloudRecNowUs() / 1000);
    platformMutexUnlock(cloudRecLock);
  }
  cloudRecBackoffStep = 0;
  cloudRecSvc = SVC_UP;
  cloudRecActive = true;
  DebugTf(PSTR("cloudrec: logging as %s\r\n"), cloudRecUid);
}

static void cloudRecBackoff(const __FlashStringHelper *why, bool maxWait) {
  cloudRecPause(true);
  uint32_t wait = maxWait ? crec::BACKOFF_MAX_MS : crec::backoffMs(cloudRecBackoffStep, platformHardwareRandom());
  if (!maxWait && cloudRecBackoffStep < 255) cloudRecBackoffStep++;
  cloudRecNextProbe = millis() + wait;
  cloudRecSvc = maxWait ? SVC_UID_TAKEN : SVC_BACKOFF;
  DebugTf(PSTR("cloudrec: %s; capture paused, next attempt in %u min\r\n"), (const char *)why, (unsigned)(wait / 60000));
}

static void cloudRecWriteConfig() {
  File f = LittleFS.open("/cloudrec.cfg", "w");
  if (!f) return;
  f.printf_P(PSTR("enabled=%d\nhost=%s\nport=%u\n"), cloudRecCfg.enabled ? 1 : 0, cloudRecCfg.host, (unsigned)cloudRecCfg.port);
  f.close();
}

static void cloudRecBlocked() {                   // the maintainer blocked this gateway: the opt-in goes off
  cloudRecPause(false);
  cloudRecRemote = {crec::RM_DEFAULT, 0, 0};       // and any central telemetry mode ends with it
  cloudRecApplyRemote();
  cloudRecSvc = SVC_OFF;
  cloudRecCfg.enabled = false;
  cloudRecWriteConfig();
  DebugTf(PSTR("cloudrec: %s is blocked by the collection point; telemetry opt-in turned off\r\n"), cloudRecUid);
}

static void cloudRecTask(void *) {
  uint32_t lastClose = millis();
  for (;;) {
    platformTaskDelay(500);
    if (cloudRecSvc == SVC_OFF) break;            // blocked: no further connection, the task ends
    if (!isNetworkUp()) {                         // own network down: pause, no backoff step
      if (cloudRecSvc == SVC_UP) { cloudRecPause(true); cloudRecSvc = SVC_PROBE; cloudRecNextProbe = millis(); }
      continue;
    }
    if (cloudRecSvc != SVC_UP) {
      if ((int32_t)(millis() - cloudRecNextProbe) < 0 || time(nullptr) < 1700000000) continue;   // and wait for NTP
      if (!cloudRecSecret[0]) continue;
      bool blocked;
      int st = cloudRecProbe(blocked);
      if (blocked) cloudRecBlocked();
      else if (st == 200) { cloudRecResume(); lastClose = millis(); }
      else if (st == 409) cloudRecBackoff(F("uid enrolled with another secret (delete it on the server)"), true);
      else cloudRecBackoff(F("service unreachable"), false);
      continue;
    }
    cloudRecApplyRemote();                        // a mode past its mode_until ends here, within a second
    if (millis() - cloudRecLastRefresh >= CLOUDREC_REFRESH_MS) {
      bool blocked;
      int st = cloudRecRefresh(blocked);
      if (blocked) { cloudRecBlocked(); continue; }
      if (crec::classify(st) == crec::OUT_AUTH) cloudRecToken[0] = 0;          // log in again at the next upload
      else if (crec::classify(st) == crec::OUT_DOWN) { cloudRecBackoff(F("token refresh failed"), false); continue; }
      cloudRecLastRefresh = millis();
    }
    if (millis() - lastClose >= CLOUDREC_INTERVAL_MS) {
      lastClose = millis();
      if (platformMutexLock(cloudRecLock, 200)) {
        cloudRecCloseLocked(cloudRecNowUs() / 1000);
        platformMutexUnlock(cloudRecLock);
      }
    }
    crec::Slot *q = crec::slotOldest(cloudRecSlots, 2);
    if (!q) continue;
    bool blocked;
    crec::Outcome o = cloudRecUpload(*q, blocked);
    if (blocked) cloudRecBlocked();
    else if (o == crec::OUT_DOWN || o == crec::OUT_AUTH) cloudRecBackoff(F("upload failed"), false);
    else q->busy = false;                         // stored, or a resend of a stored seq
  }
  platformTaskExit();
}

// ---------------------------------------------------------------- setup

static void cloudRecReadConfig() {
  File f = LittleFS.open("/cloudrec.cfg", "r");
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
    if (!strcmp_P(line, PSTR("enabled"))) cloudRecCfg.enabled = atoi(v) == 1;
    else if (!strcmp_P(line, PSTR("host"))) strlcpy(cloudRecCfg.host, v, sizeof(cloudRecCfg.host));
    else if (!strcmp_P(line, PSTR("port"))) cloudRecCfg.port = (uint16_t)atoi(v);
  }
  f.close();
}

// The gateway's identity: uid from the factory MAC, and a secret that is made once and kept in NVS, so a
// filesystem flash does not cost the gateway its enrollment.
static void cloudRecIdentity() {
  uint8_t mac[6];
  platformGetMacAddress(mac);
  snprintf_P(cloudRecUid, sizeof(cloudRecUid), PSTR("otgw-%02x%02x%02x%02x%02x%02x"), mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  Preferences prefs;
  if (!prefs.begin("cloudrec", false)) return;
  if (prefs.getString("secret", cloudRecSecret, sizeof(cloudRecSecret)) != sizeof(cloudRecSecret) - 1) {
    for (size_t i = 0; i < sizeof(cloudRecSecret) - 1; i += 8)
      snprintf_P(cloudRecSecret + i, 9, PSTR("%08lx"), (unsigned long)platformHardwareRandom());
    prefs.putString("secret", cloudRecSecret);
  }
  prefs.end();
}

void cloudRecSetup() {
  cloudRecReadConfig();
  if (!cloudRecCfg.enabled || !cloudRecCfg.host[0]) {
    DebugTln(F("cloudrec: off (no /cloudrec.cfg with enabled=1)"));
    return;
  }
  cloudRecIdentity();
  cloudRecSlots[1].buf = (uint8_t *)platformPsramAllocOnce(crec::REC_MAX_BYTES);   // nullptr without PSRAM
  cloudRecLock = platformMutexCreate();
  cloudRec.begin(cloudRecNowUs() / 1000);
  cloudRecSvc = SVC_PROBE;                          // capture starts after the first login says it may
  cloudRecNextProbe = millis();
  if (!platformTaskCreatePinned(cloudRecTask, "cloudrec", CLOUDREC_TASK_STACK, nullptr, 1)) {
    DebugTln(F("cloudrec: task create failed"));
    return;
  }
  DebugTf(PSTR("cloudrec: on, %s:%u as %s, %d send slot(s), heap %u, block %u\r\n"), cloudRecCfg.host,
          (unsigned)cloudRecCfg.port, cloudRecUid, cloudRecSlots[1].buf ? 2 : 1, (unsigned)platformFreeHeap(),
          (unsigned)platformMaxFreeBlock());
}
