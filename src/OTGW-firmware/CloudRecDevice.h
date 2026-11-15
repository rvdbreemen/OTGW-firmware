/*
***************************************************************************
**  Program  : CloudRecDevice.h
**  (Implementation in a header: the Arduino builder generates prototypes for every .ino function before the
**  includes, which breaks on the NetworkClientSecure and crec types. CloudRec.ino only includes this file.)
**  TASK-1963 proof of concept (branch research/cloud-recording only, never published): the cloud recorder.
**
**  Records the OT frames, the SATTICK lines and the debug output into one mix record (CloudRecCodec.h, 4 KB),
**  closes it every 60 s or when a section is full, and sends it from its own task as an HTTPS POST to the
**  PocketBase collection point (tools/cloud-recorder/pocketbase). Certificate verification against four
**  pinned roots. Off unless /cloudrec.cfg on LittleFS says enabled=1:
**
**      enabled=1
**      host=rec.example.nl
**      port=443
**      email=gw-1@devices.invalid
**      password=<the device password>
**
**  Built only in the esp32-combo-tlsdyn env (HAS_CLOUD_REC), whose IDF libraries use dynamic mbedTLS buffers.
**  Capture points never block: when the recorder is busy an item is counted as dropped.
**  Not recorded: output that bypasses the Debug* macros (e.g. the telnet banner).
***************************************************************************
*/

#pragma once

#include <NetworkClientSecure.h>
#include <sys/time.h>
#include "CloudRecCodec.h"

// Roots that sign the certificates Cloudflare serves for a proxied hostname (Google Trust Services, Let's
// Encrypt). SHA-256 fingerprints checked against the system store on 2026-11-15:
//   GTS Root R1  D9:47:43:2A:BD:E7:B7:FA:90:FC:2E:6B:59:10:1B:12:80:E0:E1:C7:E4:E4:0F:A3:C6:88:7F:FF:57:A7:F4:CF
//   GTS Root R4  34:9D:FA:40:58:C5:E2:63:12:3B:39:8A:E7:95:57:3C:4E:13:13:C8:3F:E6:8F:93:55:6C:D5:E8:03:1B:3C:7D
//   ISRG Root X1 96:BC:EC:06:26:49:76:F3:74:60:77:9A:CF:28:C5:A7:CF:E8:A3:C0:AA:E1:1A:8F:FC:EE:05:C0:BD:DF:08:C6
//   ISRG Root X2 69:72:9B:8E:15:A8:6E:FC:17:7A:57:AF:B7:17:1D:FC:64:AD:D2:8C:2F:CA:8C:F1:50:7E:34:45:3C:CB:14:70
#include "CloudRecRoots.h"

static const uint32_t CLOUDREC_INTERVAL_MS = 60000;
static const uint32_t CLOUDREC_TASK_STACK  = 10240;

struct CloudRecConfig {
  bool enabled = false;
  char host[64] = "";
  uint16_t port = 443;
  char email[64] = "";
  char password[64] = "";
};

static CloudRecConfig cloudRecCfg;
static crec::Recorder cloudRec;                     // the open record (about 13.5 KB, static: ADR-053)
static uint8_t  cloudRecSendBuf[crec::REC_MAX_BYTES];
static size_t   cloudRecSendLen = 0;
static int64_t  cloudRecSendT0 = 0;
static uint64_t cloudRecSendSeq = 0;
static volatile bool cloudRecSendBusy = false;
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

// ---------------------------------------------------------------- record queue (caller holds cloudRecLock)

static void cloudRecCloseLocked(int64_t reopenMs) {
  if (cloudRec.open && !cloudRec.empty()) {
    uint64_t seq = cloudRecSeqNext++;
    if (cloudRecSendBusy) {
      cloudRecDroppedRecords++;                     // the previous record is still on its way: this one is lost
    } else {
      cloudRecSendT0 = cloudRec.t0Ms;
      cloudRecSendLen = cloudRec.close(cloudRecSendBuf, sizeof(cloudRecSendBuf));
      cloudRecSendSeq = seq;
      cloudRecSendBusy = cloudRecSendLen > 0;
    }
  }
  cloudRec.begin(reopenMs);
}

template <typename F> static void cloudRecAppend(int64_t tMs, F f) {
  if (!cloudRecActive) return;
  if (!platformMutexTryLock(cloudRecLock)) { cloudRecDroppedItems++; return; }
  if (!f()) {
    cloudRecCloseLocked(tMs);
    if (!f()) cloudRecDroppedItems++;
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
      if (cloudRec.dbg.full) { cloudRec.dbg.full = false; cloudRecDroppedItems++; }
    }
    recBolPending = false;
  } else {
    size_t mark = cloudRec.dbgMark();
    crec::dbgRaw(cloudRec.dbg, recTee, recTeeLen);
    if (cloudRec.dbg.full) {
      cloudRec.dbg.n = mark; cloudRec.dbg.full = false;
      cloudRecCloseLocked(cloudRecNowUs() / 1000);
      crec::dbgRaw(cloudRec.dbg, recTee, recTeeLen);
      if (cloudRec.dbg.full) { cloudRec.dbg.full = false; cloudRecDroppedItems++; }
    }
  }
  recTeeLen = 0;
}

void cloudRecTeeFlush() {
  if (!cloudRecActive || !recTeeLen) return;
  if (!platformMutexTryLock(cloudRecLock)) { recTeeLen = 0; cloudRecDroppedItems++; return; }
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
  if (!platformMutexTryLock(cloudRecLock)) { cloudRecDroppedItems++; recBolPending = false; return; }
  int64_t tUs = recBolPending ? recBolUs : cloudRecNowUs();
  if (!strncmp(printed, "SATTICK ", 8)) {
    for (int fi = 0; fi < 2; fi++) {
      if (strcmp(fmt, crec::SAT_FORMATS[fi])) continue;
      if (!cloudRec.satTick(tUs / 1000, fi, printed)) {
        cloudRecCloseLocked(tUs / 1000);
        if (!cloudRec.satTick(tUs / 1000, fi, printed)) cloudRecDroppedItems++;
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
      if (attempt == 0) cloudRecCloseLocked(tUs / 1000); else cloudRecDroppedItems++;
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

void cloudRecOtFrame(char letter, uint32_t frame) {
  int64_t tMs = cloudRecNowUs() / 1000;
  cloudRecAppend(tMs, [&] { return cloudRec.otFrame(tMs, letter, frame); });
}

// ---------------------------------------------------------------- upload

static int cloudRecReadStatus(NetworkClientSecure &c, char *body, size_t bodyCap) {
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

static void cloudRecWriteHead(NetworkClientSecure &c, const char *path, size_t len, bool auth) {
  char h[256];
  snprintf_P(h, sizeof(h), PSTR("POST %s HTTP/1.1\r\nHost: %s\r\nContent-Type: application/json\r\nContent-Length: %u\r\n"
                                "Connection: keep-alive\r\n"), path, cloudRecCfg.host, (unsigned)len);
  c.print(h);
  if (auth) { c.print(F("Authorization: ")); c.print(cloudRecToken); c.print(F("\r\n")); }
  c.print(F("\r\n"));
}

static bool cloudRecLogin(NetworkClientSecure &c) {
  char body[200];
  int n = snprintf_P(body, sizeof(body), PSTR("{\"identity\":\"%s\",\"password\":\"%s\"}"), cloudRecCfg.email, cloudRecCfg.password);
  cloudRecWriteHead(c, "/api/collections/devices/auth-with-password", (size_t)n, false);
  c.print(body);
  static char resp[1024];                       // the login reply: the device record and the token
  int st = cloudRecReadStatus(c, resp, sizeof(resp));
  if (st != 200) { cloudRecLastStatus = st; return false; }
  return cloudRecJsonString(resp, nullptr, "\"token\":\"", cloudRecToken, sizeof(cloudRecToken)) &&
         cloudRecJsonString(resp, "\"record\"", "\"id\":\"", cloudRecDevId, sizeof(cloudRecDevId));
}

static void cloudRecWriteBase64(NetworkClientSecure &c, const uint8_t *d, size_t n) {
  static const char tab[] PROGMEM = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  char out[256];
  size_t o = 0;
  for (size_t i = 0; i < n; i += 3) {
    uint32_t v = (uint32_t)d[i] << 16 | (i + 1 < n ? (uint32_t)d[i + 1] << 8 : 0) | (i + 2 < n ? d[i + 2] : 0);
    out[o++] = tab[(v >> 18) & 63];
    out[o++] = tab[(v >> 12) & 63];
    out[o++] = i + 1 < n ? tab[(v >> 6) & 63] : '=';
    out[o++] = i + 2 < n ? tab[v & 63] : '=';
    if (o + 4 > sizeof(out)) { c.write((const uint8_t *)out, o); o = 0; }
  }
  if (o) c.write((const uint8_t *)out, o);
}

static bool cloudRecUpload() {
  uint32_t heapBefore = platformFreeHeap(), blockBefore = platformMaxFreeBlock();
  uint32_t t = millis();
  NetworkClientSecure c;
  c.setCACert(CLOUDREC_ROOTS);
  c.setTimeout(10);
  if (!c.connect(cloudRecCfg.host, cloudRecCfg.port)) {
    cloudRecFailed++; cloudRecLastStatus = 0;
    DebugTf(PSTR("cloudrec: connect to %s:%u failed (heap %u, block %u)\r\n"), cloudRecCfg.host,
            (unsigned)cloudRecCfg.port, (unsigned)platformFreeHeap(), (unsigned)platformMaxFreeBlock());
    return false;
  }
  uint32_t heapTls = platformFreeHeap(), blockTls = platformMaxFreeBlock();
  if (heapTls < cloudRecHeapLow) cloudRecHeapLow = heapTls;
  if (blockTls < cloudRecBlockLow) cloudRecBlockLow = blockTls;
  if (!cloudRecToken[0] && !cloudRecLogin(c)) {
    c.stop(); cloudRecFailed++;
    DebugTf(PSTR("cloudrec: login failed, HTTP %u\r\n"), (unsigned)cloudRecLastStatus);
    return false;
  }
  char head[200];
  int hl = snprintf_P(head, sizeof(head), PSTR("{\"device\":\"%s\",\"stream\":\"mix\",\"seq\":%llu,\"t0\":%lld,\"fmt\":4,\"payload\":\""),
                      cloudRecDevId, (unsigned long long)cloudRecSendSeq, (long long)cloudRecSendT0);
  size_t b64 = 4 * ((cloudRecSendLen + 2) / 3);
  cloudRecWriteHead(c, "/api/collections/batches/records?fields=id", (size_t)hl + b64 + 2, true);
  c.write((const uint8_t *)head, hl);
  cloudRecWriteBase64(c, cloudRecSendBuf, cloudRecSendLen);
  c.print(F("\"}"));
  int st = cloudRecReadStatus(c, nullptr, 0);
  c.stop();
  cloudRecLastStatus = st;
  cloudRecLastMs = millis() - t;
  bool ok = st == 200 || st == 400;                 // 400: this seq is already stored (a resend), so it is done
  if (st == 401 || st == 403) cloudRecToken[0] = 0;
  if (ok) cloudRecSent++; else cloudRecFailed++;
  DebugTf(PSTR("cloudrec: seq %llu %u B HTTP %d in %u ms; heap before %u/%u, with TLS %u/%u, low %u/%u, min %u\r\n"),
          (unsigned long long)cloudRecSendSeq, (unsigned)cloudRecSendLen, st, (unsigned)cloudRecLastMs,
          (unsigned)heapBefore, (unsigned)blockBefore, (unsigned)heapTls, (unsigned)blockTls,
          (unsigned)cloudRecHeapLow, (unsigned)cloudRecBlockLow, (unsigned)platformMinFreeHeap());
  return ok;
}

static void cloudRecTask(void *) {
  uint32_t lastClose = millis(), nextTry = 0;
  uint8_t tries = 0;
  for (;;) {
    platformTaskDelay(500);
    if (millis() - lastClose >= CLOUDREC_INTERVAL_MS) {
      lastClose = millis();
      if (platformMutexLock(cloudRecLock, 200)) {
        cloudRecCloseLocked(cloudRecNowUs() / 1000);
        platformMutexUnlock(cloudRecLock);
      }
    }
    if (!cloudRecSendBusy || (int32_t)(millis() - nextTry) < 0 || !isNetworkUp()) continue;
    if (cloudRecUpload()) {
      cloudRecSendBusy = false; tries = 0;
    } else if (++tries >= 3) {
      cloudRecSendBusy = false; tries = 0; cloudRecDroppedRecords++;
    } else {
      nextTry = millis() + 5000u * tries;
    }
  }
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
    else if (!strcmp_P(line, PSTR("email"))) strlcpy(cloudRecCfg.email, v, sizeof(cloudRecCfg.email));
    else if (!strcmp_P(line, PSTR("password"))) strlcpy(cloudRecCfg.password, v, sizeof(cloudRecCfg.password));
  }
  f.close();
}

void cloudRecSetup() {
  cloudRecReadConfig();
  if (!cloudRecCfg.enabled || !cloudRecCfg.host[0]) {
    DebugTln(F("cloudrec: off (no /cloudrec.cfg with enabled=1)"));
    return;
  }
  cloudRecLock = platformMutexCreate();
  timeval tv;
  gettimeofday(&tv, nullptr);
  cloudRecSeqNext = (uint64_t)tv.tv_sec << 20;      // consecutive within a boot, a jump at a reboot
  cloudRec.begin(cloudRecNowUs() / 1000);
  if (!platformTaskCreatePinned(cloudRecTask, "cloudrec", CLOUDREC_TASK_STACK, nullptr, 1)) {
    DebugTln(F("cloudrec: task create failed"));
    return;
  }
  cloudRecActive = true;
  DebugTf(PSTR("cloudrec: on, %s:%u, heap %u, block %u\r\n"), cloudRecCfg.host, (unsigned)cloudRecCfg.port,
          (unsigned)platformFreeHeap(), (unsigned)platformMaxFreeBlock());
}

