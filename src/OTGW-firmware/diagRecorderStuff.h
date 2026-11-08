/*
***************************************************************************
**  Program  : diagRecorderStuff.h
**  Version  : v2.0.0-beta.149
**
**  Copyright (c) 2021-2026 Robert van den Breemen
**
**  Local diagnostic recorder, step 1 (TASK-1665, ADR-213): the record format,
**  the ring, the start rule and the redaction helpers. Pure C++ with no
**  Arduino or platform include, so tests/ and test/host/ compile this real
**  header. The glue (platform allocation, mutex, capture points, setting) is in
**  diagRecorderStuff.ino. Header naming: ADR-081.
**
**  FORMAT (version 1). A recording is a sequence of 4096-byte blocks.
**    Block header, 16 bytes, little endian:
**      block_seq u32 | uptime_ms u32 (of the first record) | epoch u32 (0 = clock
**      not set) | first_record_seq u32
**    Record: type u8 | dt LEB128 (ms since the previous record, 0 for the first
**    record of a block) | payload. A record never crosses a block boundary. After the
**    last record the block is zero filled: type 0 ends the block.
**    The repeat caches are cleared at every block start, so every block decodes
**    on its own (an overwritten block never orphans a later one).
**
**  THREADS (ADR-090 contract). Capture points run on the loop, PIC, async_tcp
**  and BLE-reader tasks. Every state change of the recorder happens inside the
**  env.tryLock() / env.unlock() pair. tryLock() must not wait: when it fails the
**  record is dropped and counted, and the next successful append writes a Gap
**  record first. The lock is held for one encode (at most ~270 bytes copied
**  and, once per block, a 4 KiB clear). Nothing inside the lock calls out
**  (no malloc, no log, no other lock). start() and stop() run on the loop task
**  and also take the lock around the state they change. `active` is a volatile
**  flag read without the lock by the capture points; it is the cheap no-op
**  test when no recording runs. A capture point that reads `active` true and
**  then races with stop() finds `ring == nullptr` under the lock and drops.
**
**  TERMS OF USE: GNU GPLv3. See bottom of file.
***************************************************************************
*/

#ifndef DIAGRECORDERSTUFF_H
#define DIAGRECORDERSTUFF_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>

// The capture points stay out of line: the call sites in the hot paths test `active` first and
// the encoder body is shared.
#if defined(__GNUC__)
#define DIAG_NOINLINE __attribute__((noinline))
#elif defined(_MSC_VER)
#define DIAG_NOINLINE __declspec(noinline)
#else
#define DIAG_NOINLINE
#endif

// ---- geometry (ADR-213 decision 2 and 3) ------------------------------------
#define DIAG_REC_FORMAT_VERSION   1
#define DIAG_REC_BLOCK_SIZE       4096u
#define DIAG_REC_BLOCK_HDR        16u
#define DIAG_REC_PSRAM_BLOCKS     384u                    // 1.5 MB
#define DIAG_REC_HEAP_BLOCKS      1u                      // 4 KB, no PSRAM
#define DIAG_REC_MIN_FREE_AFTER   5120u                   // ADR-030 rung: refuse below this
#define DIAG_REC_TEXT_MAX         254u                    // longest text stored whole; 255 = cut
#define DIAG_REC_TEXT_CUT         255u
#define DIAG_REC_GAP_ROOM         11u                     // type + dt + varint count

// Repeat caches (direct mapped, power of two). Sizes per board class: a PSRAM board
// sees ~100 OT keys and ~260 topics per block; a 4 KB heap block only a few dozen.
#define DIAG_REC_HEAP_OT_BITS     6u                      //  64 entries x 6 B
#define DIAG_REC_HEAP_MQ_BITS     6u                      //  64 entries x 6 B
#define DIAG_REC_PSRAM_OT_BITS    8u                      // 256 entries x 6 B
#define DIAG_REC_PSRAM_MQ_BITS    9u                      // 512 entries x 6 B

// ---- record types -----------------------------------------------------------
enum DiagRecType : uint8_t {
  DIAG_T_END         = 0,    // zero fill: end of the block
  DIAG_T_OT_FRAME    = 1,    // +src 0..4 (T,B,R,A,E): 1..5. Payload 4 bytes (the frame, big endian)
  DIAG_T_OT_REPEAT   = 6,    // +src: 6..10. Payload MsgID u8
  DIAG_T_PIC_CMD     = 11,   // text
  DIAG_T_OTD_CMD     = 12,   // text
  DIAG_T_RESPONSE    = 13,   // text
  DIAG_T_MQTT_PUB    = 14,   // topic id u16, text (value)
  DIAG_T_MQTT_REPEAT = 15,   // topic id u16
  DIAG_T_MQTT_IN     = 16,   // text (topic suffix), text (value)
  DIAG_T_REST_IN     = 17,   // route id u8, method u8, text (body)
  DIAG_T_WEATHER     = 18,   // int16 x100: temperature, humidity, wind
  DIAG_T_BLE         = 19,   // slot u8, temp int16 x100, humidity int16 x100, battery u8, rssi int8
  DIAG_T_SETTING     = 20,   // setting id u16, text (value, redacted)
  DIAG_T_REBOOT      = 21,   // boot count u32, text (reset reason), text (crash summary)
  DIAG_T_GAP         = 22,   // dropped record count, LEB128
  DIAG_T_TIME_SYNC   = 23    // epoch u32
};
// Text field: len u8 + bytes. len 0..254 = the whole text. len 255 = the text was cut and 254 bytes follow.

// ---- why a start was refused (reported by /api/v2/diag/recording/status) -----
enum DiagStartResult : uint8_t {
  DIAG_START_OK          = 0,
  DIAG_START_ALREADY     = 1,    // a recording runs: not an error
  DIAG_REFUSED_HEAP      = 2,    // free heap after the allocation would be below 5120 B
  DIAG_REFUSED_ALLOC     = 3     // the allocation failed
};

inline const char* diagStartReasonText(DiagStartResult r) {
  switch (r) {
    case DIAG_START_OK:      return "";
    case DIAG_START_ALREADY: return "already running";
    case DIAG_REFUSED_HEAP:  return "free heap would drop below 5120 B";
    case DIAG_REFUSED_ALLOC: return "allocation failed";
  }
  return "";
}

// settings.diag (ADR-051): persisted as "DiagRecordingOn" in settings.ini.
struct DiagSettingsSection {
  bool bRecordingOn = false;      // a recording runs, or ran when the unit rebooted; resume at boot
};

// ---- environment (the glue fills it; tests fill it with doubles) -------------
struct DiagEnv {
  uint32_t (*millis)();               // uptime in ms
  uint32_t (*epoch)();                // unix time, 0 while the clock is not set
  uint32_t (*freeHeap)();             // platformFreeHeap()
  void*    (*allocHeap)(size_t);      // malloc
  void     (*freeHeapBlock)(void*);   // free
  void*    (*allocPsramOnce)(size_t); // platformPsramAllocOnce(); never freed
  bool     (*tryLock)();              // capture points: must not wait
  bool     (*lockWait)(uint32_t ms);  // start() and stop() on the loop task: may wait a few ms
  void     (*unlock)();
  bool     hasPsram;                  // platformHasPsram()
};

// ---- small helpers ----------------------------------------------------------
static inline uint8_t diagPutVarint(uint8_t* p, uint32_t v) {
  uint8_t n = 0;
  while (v >= 0x80u) { p[n++] = (uint8_t)(v | 0x80u); v >>= 7; }
  p[n++] = (uint8_t)v;
  return n;                            // 1..5
}

static inline uint32_t diagFnv32(const uint8_t* d, size_t n, uint32_t h = 2166136261u) {
  for (size_t i = 0; i < n; i++) { h ^= d[i]; h *= 16777619u; }
  return h;
}

// 16-bit id of a name (MQTT topic suffix, setting field). The id table is implicit and fixed by
// the format version: FNV-1a over the ASCII-lowercased name, folded to 16 bits. 0xFFFF is
// reserved (empty cache slot).
static inline uint16_t diagNameId(const char* s) {
  uint32_t h = 2166136261u;
  for (; *s; s++) {
    uint8_t c = (uint8_t)*s;
    if (c >= 'A' && c <= 'Z') c = (uint8_t)(c + 32);
    h ^= c; h *= 16777619u;
  }
  uint16_t id = (uint16_t)((h >> 16) ^ (h & 0xFFFFu));
  return (id == 0xFFFFu) ? (uint16_t)0xFFFEu : id;
}

// A float as value x 100 in an int16, rounded half away from zero and clamped. NaN gives INT16_MIN
// (no value). Weather and BLE capture points.
static inline int16_t diagX100(float v) {
  if (v != v) return INT16_MIN;
  float x = v * 100.0f;
  if (x > 32767.0f) x = 32767.0f;
  if (x < -32767.0f) x = -32767.0f;
  return (int16_t)(x >= 0.0f ? x + 0.5f : x - 0.5f);
}

static inline int8_t diagOtSource(char c) {
  switch (c) { case 'T': return 0; case 'B': return 1; case 'R': return 2; case 'A': return 3; case 'E': return 4; }
  return -1;
}

// ---- redaction (ADR-213 decision 6; the download is open, so this is mandatory) --
static inline char diagLower(char c) { return (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c; }

static inline bool diagContainsNoCase(const char* hay, size_t hayLen, const char* needle) {
  size_t nl = strlen(needle);
  if (nl == 0 || hayLen < nl) return false;
  for (size_t i = 0; i + nl <= hayLen; i++) {
    size_t j = 0;
    while (j < nl && diagLower(hay[i + j]) == needle[j]) j++;
    if (j == nl) return true;
  }
  return false;
}

// A setting or key name whose value must never enter the ring: passwords, keys, tokens, MQTT
// credentials, webhook URLs (they carry tokens) and BLE MAC addresses.
static inline bool diagNameIsSecret(const char* name, size_t len) {
  static const char* const kWords[] = { "pass", "key", "token", "secret", "user", "mac", "url", "psk", "auth" };
  for (size_t i = 0; i < sizeof(kWords) / sizeof(kWords[0]); i++)
    if (diagContainsNoCase(name, len, kWords[i])) return true;
  return false;
}

// Location settings end in "lat" or "lon" (SATweatherlat, SATweatherlon).
static inline bool diagNameIsLocation(const char* name, size_t len) {
  if (len < 3) return false;
  const char* e = name + len - 3;
  return (diagLower(e[0]) == 'l' && diagLower(e[1]) == 'a' && diagLower(e[2]) == 't') ||
         (diagLower(e[0]) == 'l' && diagLower(e[1]) == 'o' && diagLower(e[2]) == 'n');
}

// Round a decimal string to 0.1 (half away from zero) without floating point. Writes "-12.3".
// Returns false when the text is not a plain decimal number.
static inline bool diagRoundTenth(const char* s, char* out, size_t cap) {
  size_t i = 0;
  bool neg = false;
  if (s[i] == '-') { neg = true; i++; } else if (s[i] == '+') { i++; }
  if (s[i] < '0' || s[i] > '9') return false;
  uint32_t ip = 0;
  while (s[i] >= '0' && s[i] <= '9') { if (ip < 100000u) ip = ip * 10u + (uint32_t)(s[i] - '0'); i++; }
  uint32_t d1 = 0, d2 = 0;
  if (s[i] == '.') {
    i++;
    if (s[i] >= '0' && s[i] <= '9') { d1 = (uint32_t)(s[i] - '0'); i++; }
    if (s[i] >= '0' && s[i] <= '9') { d2 = (uint32_t)(s[i] - '0'); i++; }
    while (s[i] >= '0' && s[i] <= '9') i++;
  }
  if (s[i] != '\0') return false;
  uint32_t tenths = ip * 10u + d1 + (d2 >= 5u ? 1u : 0u);
  char tmp[16];
  int n = 0;
  uint32_t whole = tenths / 10u, frac = tenths % 10u;
  char w[12]; int wn = 0;
  do { w[wn++] = (char)('0' + whole % 10u); whole /= 10u; } while (whole && wn < 11);
  if (neg && tenths != 0) tmp[n++] = '-';
  while (wn) tmp[n++] = w[--wn];
  tmp[n++] = '.'; tmp[n++] = (char)('0' + frac); tmp[n] = '\0';
  if ((size_t)n + 1 > cap) return false;
  memcpy(out, tmp, (size_t)n + 1);
  return true;
}

static inline void diagRedactBody(char* buf, size_t len);

// The value a Setting record may carry. Secrets become "set" or "empty"; location becomes 0.1 degree.
static inline void diagRedactSetting(const char* field, const char* value, char* out, size_t cap) {
  if (cap == 0) return;
  const size_t fl = strlen(field);
  if (diagNameIsSecret(field, fl)) {
    const char* r = (value && value[0]) ? "set" : "empty";
    size_t n = strlen(r); if (n + 1 > cap) n = cap - 1;
    memcpy(out, r, n); out[n] = '\0';
    return;
  }
  if (diagNameIsLocation(field, fl) && value) {
    if (diagRoundTenth(value, out, cap)) return;
    const char* r = "set";                      // not a plain number: do not copy it
    size_t n = strlen(r); if (n + 1 > cap) n = cap - 1;
    memcpy(out, r, n); out[n] = '\0';
    return;
  }
  size_t n = value ? strlen(value) : 0;
  if (n + 1 > cap) n = cap - 1;
  if (n) memcpy(out, value, n);
  out[n] = '\0';
  // A free-text value (a webhook payload template) can itself hold "token":"..." or key=...
  diagRedactBody(out, n);
}

// Blank the value that follows a secret-looking key in a JSON or form body, in place.
// Handles  "key":"value"  "key": value  key=value&...  Best effort on the shapes the REST and MQTT
// command paths accept; it never lengthens the text.
static inline void diagRedactBody(char* buf, size_t len) {
  size_t i = 0;
  while (i < len) {
    // a key token: letters, digits, '_' or '-'
    size_t ks = i;
    while (i < len && ((buf[i] >= 'a' && buf[i] <= 'z') || (buf[i] >= 'A' && buf[i] <= 'Z') ||
                       (buf[i] >= '0' && buf[i] <= '9') || buf[i] == '_' || buf[i] == '-')) i++;
    size_t ke = i;
    if (ke == ks) { i++; continue; }
    if (!diagNameIsSecret(buf + ks, ke - ks)) continue;
    size_t j = ke;
    if (j < len && buf[j] == '"') j++;                       // closing quote of a JSON key
    while (j < len && buf[j] == ' ') j++;
    if (j >= len || (buf[j] != ':' && buf[j] != '=')) continue;
    j++;
    while (j < len && buf[j] == ' ') j++;
    if (j < len && buf[j] == '"') {                          // quoted value
      j++;
      while (j < len && buf[j] != '"') { buf[j] = '*'; j++; }
    } else {                                                 // bare value
      while (j < len && buf[j] != ',' && buf[j] != '}' && buf[j] != '&' && buf[j] != ' ') { buf[j] = '*'; j++; }
    }
    i = j;
  }
}

// ---- the recorder -------------------------------------------------------------
struct DiagRecorder {
  // --- state read without the lock (see THREADS) ---
  volatile bool active = false;
  // Set by the caller around a command that came from the port 25238 bridge: ADR-213 does not record bridge
  // traffic in version 1. Read without the lock; a command from another task inside that window is missed.
  volatile bool bridgeLine = false;
  volatile uint32_t pendingDrops = 0;       // approximate: incremented without the lock on a failed tryLock

  // --- state under the lock ---
  const DiagEnv* env = nullptr;
  uint8_t*  mem = nullptr;                  // ring blocks, then the caches
  uint8_t*  ring = nullptr;                 // == mem while recording
  bool      memIsPsram = false;
  uint32_t  nBlocks = 0;
  uint32_t  curSeq = 0;                     // block_seq of the block being written
  uint32_t  used = 0;                       // bytes used in the current block (0 = no block open)
  bool      haveBlock = false;
  uint32_t  recSeq = 0;                     // sequence of the next record
  uint32_t  lastRecMs = 0;                  // uptime of the previous record in this block
  uint32_t  droppedTotal = 0;               // records dropped over the recording (gap records carry the same counts)
  DiagStartResult lastStart = DIAG_START_OK;
  uint32_t  psramMemBytes = 0;              // size of the never-freed PSRAM block, 0 = not allocated

  // caches (point into mem)
  uint16_t* otKey = nullptr; uint32_t* otVal = nullptr; uint8_t otBits = 0;
  uint16_t* mqKey = nullptr; uint32_t* mqVal = nullptr; uint8_t mqBits = 0;

  // ---------------------------------------------------------------- start/stop
  static size_t cacheBytes(uint8_t otBitsN, uint8_t mqBitsN) {
    return ((size_t)1 << otBitsN) * 6u + ((size_t)1 << mqBitsN) * 6u;
  }

  // Start a recording on the loop task. Obeys the ADR-213 start rule; the same call serves a manual
  // start and a resume after a reboot.
  DiagStartResult start(const DiagEnv& e) {
    if (active) return DIAG_START_ALREADY;
    env = &e;
    const bool psram = e.hasPsram;
    const uint32_t blocks = psram ? DIAG_REC_PSRAM_BLOCKS : DIAG_REC_HEAP_BLOCKS;
    const uint8_t ob = psram ? DIAG_REC_PSRAM_OT_BITS : DIAG_REC_HEAP_OT_BITS;
    const uint8_t mb = psram ? DIAG_REC_PSRAM_MQ_BITS : DIAG_REC_HEAP_MQ_BITS;
    const size_t total = (size_t)blocks * DIAG_REC_BLOCK_SIZE + cacheBytes(ob, mb);
    uint8_t* m = nullptr;
    if (mem && !memIsPsram) { e.freeHeapBlock(mem); mem = nullptr; ring = nullptr; }   // left by a stop() that lost the lock
    if (psram) {
      if (mem && memIsPsram) {
        m = mem;                                            // allocated by an earlier start, never freed
      } else {
        m = (uint8_t*)e.allocPsramOnce(total);
        if (m) psramMemBytes = (uint32_t)total;
      }
      if (!m) return lastStart = DIAG_REFUSED_ALLOC;
    } else {
      const uint32_t freeNow = e.freeHeap();
      if (freeNow < total + DIAG_REC_MIN_FREE_AFTER) return lastStart = DIAG_REFUSED_HEAP; // DIAG-GUARD-HEAP
      m = (uint8_t*)e.allocHeap(total);
      if (!m) return lastStart = DIAG_REFUSED_ALLOC;
    }
    if (!e.lockWait(50)) {                                  // only a capture point holds it, for microseconds
      if (!psram) e.freeHeapBlock(m);
      return lastStart = DIAG_REFUSED_ALLOC;
    }
    mem = m; memIsPsram = psram;
    ring = m;
    nBlocks = blocks;
    uint8_t* c = m + (size_t)blocks * DIAG_REC_BLOCK_SIZE;
    otBits = ob; mqBits = mb;
    otKey = (uint16_t*)c; c += ((size_t)1 << ob) * sizeof(uint16_t);
    otVal = (uint32_t*)c; c += ((size_t)1 << ob) * sizeof(uint32_t);
    mqKey = (uint16_t*)c; c += ((size_t)1 << mb) * sizeof(uint16_t);
    mqVal = (uint32_t*)c;
    curSeq = 0; used = 0; haveBlock = false; recSeq = 0; lastRecMs = 0; droppedTotal = 0; pendingDrops = 0;
    clearCaches();
    lastStart = DIAG_START_OK;
    active = true;
    e.unlock();
    return DIAG_START_OK;
  }

  // Resume after a reboot: the same start rule, and the Reboot record is the first record.
  DiagStartResult resume(const DiagEnv& e, uint32_t bootCount, const char* resetReason, const char* crashSummary) {
    DiagStartResult r = start(e);
    if (r == DIAG_START_OK) recReboot(bootCount, resetReason, crashSummary);
    return r;
  }

  // Stop on the loop task. A heap ring is freed (no ring stays allocated on a no-PSRAM board);
  // the PSRAM block stays (ADR-193 pattern) but is no longer written.
  bool stop() {
    if (!active) return true;
    active = false;                                         // capture points stop starting new appends
    if (!env) return true;
    if (!env->lockWait(50)) return false;                   // leave the ring; the next start() frees it
    uint8_t* toFree = memIsPsram ? nullptr : mem;
    if (!memIsPsram) mem = nullptr;
    ring = nullptr; haveBlock = false; used = 0;
    otKey = nullptr; otVal = nullptr; mqKey = nullptr; mqVal = nullptr;
    env->unlock();
    if (toFree) env->freeHeapBlock(toFree);
    return true;
  }

  // ---------------------------------------------------------------- reading (for the download, TASK-1666)
  // Call under the same lock the writer uses, or from a context that tolerates a block changing.
  bool     hasData() const { return ring != nullptr && haveBlock; }
  uint32_t newestSeq() const { return curSeq; }
  uint32_t oldestSeq() const { return (curSeq + 1u > nBlocks) ? curSeq + 1u - nBlocks : 0u; }
  uint32_t ringBytes() const { return nBlocks * DIAG_REC_BLOCK_SIZE; }
  const uint8_t* block(uint32_t seq) const {
    if (!ring || !haveBlock || seq > curSeq || seq < oldestSeq()) return nullptr;
    return ring + (size_t)(seq % nBlocks) * DIAG_REC_BLOCK_SIZE;
  }

  // ---------------------------------------------------------------- capture points
  // Every capture point starts with `if (!active) return;` (one volatile read when not recording).

  DIAG_NOINLINE void recOtFrame(char srcChar, uint32_t frame) {
    if (!active) return;
    const int8_t src = diagOtSource(srcChar);
    if (src < 0) return;
    const uint8_t id = (uint8_t)(frame >> 16);
    const uint16_t key = (uint16_t)(((uint16_t)src << 8) | id);
    Writer w;
    if (!w.open(*this, 10u)) return;
    const uint32_t idx = hashIdx(key, otBits);
    if (otKey[idx] == key && otVal[idx] == frame) {
      w.begin((uint8_t)(DIAG_T_OT_REPEAT + src));
      w.u8(id);
    } else {
      otKey[idx] = key; otVal[idx] = frame;
      w.begin((uint8_t)(DIAG_T_OT_FRAME + src));
      w.u8((uint8_t)(frame >> 24)); w.u8((uint8_t)(frame >> 16)); w.u8((uint8_t)(frame >> 8)); w.u8((uint8_t)frame);
    }
    w.close();
  }

  DIAG_NOINLINE void recPicCommand(const char* text, size_t len)    { recText1(DIAG_T_PIC_CMD, text, len); }
  DIAG_NOINLINE void recOtdCommand(const char* text, size_t len)    { recText1(DIAG_T_OTD_CMD, text, len); }
  DIAG_NOINLINE void recResponse(const char* text, size_t len)      { recText1(DIAG_T_RESPONSE, text, len); }

  // topic: the suffix sendMQTTData() receives (before <prefix>/value/<node>/).
  DIAG_NOINLINE void recMqttPublish(const char* topic, const char* value) {
    if (!active) return;
    const uint16_t id = diagNameId(topic);
    const size_t vlen = strlen(value);
    const uint32_t vh = diagFnv32((const uint8_t*)value, vlen) ^ (uint32_t)vlen * 2654435761u;
    Writer w;
    if (!w.open(*this, 1u + 5u + 2u + 1u + textStored(vlen))) return;
    const uint32_t idx = hashIdx(id, mqBits);
    if (mqKey[idx] == id && mqVal[idx] == vh) {
      w.begin(DIAG_T_MQTT_REPEAT);
      w.u16(id);
    } else {
      mqKey[idx] = id; mqVal[idx] = vh;
      w.begin(DIAG_T_MQTT_PUB);
      w.u16(id);
      w.text(value, vlen);
    }
    w.close();
  }

  DIAG_NOINLINE void recMqttIn(const char* topicSuffix, const char* value, size_t vlen) {
    if (!active) return;
    char tmp[DIAG_REC_TEXT_MAX + 1];
    size_t n = vlen < DIAG_REC_TEXT_MAX ? vlen : DIAG_REC_TEXT_MAX;
    memcpy(tmp, value, n); tmp[n] = '\0';
    diagRedactBody(tmp, n);
    const size_t tl = strlen(topicSuffix);
    Writer w;
    if (!w.open(*this, 1u + 5u + 1u + textStored(tl) + 1u + textStored(n))) return;
    w.begin(DIAG_T_MQTT_IN);
    w.text(topicSuffix, tl);
    w.text(tmp, n);
    if (vlen > n) w.markLastTextCut();
    w.close();
  }

  DIAG_NOINLINE void recRestIn(uint8_t routeId, uint8_t method, const char* body, size_t blen) {
    if (!active) return;
    char tmp[DIAG_REC_TEXT_MAX + 1];
    size_t n = blen < DIAG_REC_TEXT_MAX ? blen : DIAG_REC_TEXT_MAX;
    if (n) memcpy(tmp, body, n);
    tmp[n] = '\0';
    diagRedactBody(tmp, n);
    Writer w;
    if (!w.open(*this, 1u + 5u + 2u + 1u + n)) return;
    w.begin(DIAG_T_REST_IN);
    w.u8(routeId); w.u8(method);
    w.text(tmp, n);
    if (blen > n) w.markLastTextCut();
    w.close();
  }

  DIAG_NOINLINE void recWeather(int16_t tempX100, int16_t humX100, int16_t windX100) {
    if (!active) return;
    Writer w;
    if (!w.open(*this, 1u + 5u + 6u)) return;
    w.begin(DIAG_T_WEATHER); w.i16(tempX100); w.i16(humX100); w.i16(windX100);
    w.close();
  }

  // slot: 0..n of the configured sensors. The MAC address never enters the ring.
  DIAG_NOINLINE void recBle(uint8_t slot, int16_t tempX100, int16_t humX100, uint8_t battery, int8_t rssi) {
    if (!active) return;
    Writer w;
    if (!w.open(*this, 1u + 5u + 7u)) return;
    w.begin(DIAG_T_BLE); w.u8(slot); w.i16(tempX100); w.i16(humX100); w.u8(battery); w.u8((uint8_t)rssi);
    w.close();
  }

  // field/value as updateSetting() receives them; redacted here.
  DIAG_NOINLINE void recSetting(const char* field, const char* value) {
    if (!active) return;
    char red[DIAG_REC_TEXT_MAX + 1];
    diagRedactSetting(field, value ? value : "", red, sizeof(red));
    const size_t n = strlen(red);
    Writer w;
    if (!w.open(*this, 1u + 5u + 2u + 1u + n)) return;
    w.begin(DIAG_T_SETTING); w.u16(diagNameId(field)); w.text(red, n);
    w.close();
  }

  DIAG_NOINLINE void recReboot(uint32_t bootCount, const char* resetReason, const char* crashSummary) {
    if (!active) return;
    const size_t rl = strlen(resetReason ? resetReason : ""), cl = strlen(crashSummary ? crashSummary : "");
    Writer w;
    if (!w.open(*this, 1u + 5u + 4u + 1u + textStored(rl) + 1u + textStored(cl))) return;
    w.begin(DIAG_T_REBOOT); w.u32(bootCount); w.text(resetReason ? resetReason : "", rl); w.text(crashSummary ? crashSummary : "", cl);
    w.close();
  }

  DIAG_NOINLINE void recTimeSync(uint32_t epoch) {
    if (!active) return;
    Writer w;
    if (!w.open(*this, 1u + 5u + 4u)) return;
    w.begin(DIAG_T_TIME_SYNC); w.u32(epoch);
    w.close();
  }

private:
  static size_t textStored(size_t n) { return n > DIAG_REC_TEXT_MAX ? DIAG_REC_TEXT_MAX : n; }
  static uint32_t hashIdx(uint16_t key, uint8_t bits) { return ((uint32_t)key * 0x9E3779B1u) >> (32u - bits); }

  void clearCaches() {
    for (uint32_t i = 0; i < ((uint32_t)1 << otBits); i++) { otKey[i] = 0xFFFFu; otVal[i] = 0; }
    for (uint32_t i = 0; i < ((uint32_t)1 << mqBits); i++) { mqKey[i] = 0xFFFFu; mqVal[i] = 0; }
  }

  void recText1(uint8_t type, const char* text, size_t len) {
    if (!active) return;
    if (bridgeLine && (type == DIAG_T_PIC_CMD || type == DIAG_T_OTD_CMD)) return; // DIAG-GUARD-BRIDGE
    Writer w;
    if (!w.open(*this, 1u + 5u + 1u + textStored(len))) return;
    w.begin(type); w.text(text, len);
    w.close();
  }

  // One record. open() takes the lock, makes room (a new block when the current one is too full) and
  // writes a pending Gap record; close() commits the record and releases the lock. Everything between
  // runs inside the lock and only copies bytes.
  struct Writer {
    DiagRecorder* r = nullptr;
    bool locked = false;
    uint8_t* p = nullptr;          // write pointer inside the current block
    uint8_t* lastTextLen = nullptr;
    uint32_t now = 0;

    bool open(DiagRecorder& rec, size_t room) {
      r = &rec;
      if (!rec.env || !rec.env->tryLock()) { rec.pendingDrops = rec.pendingDrops + 1u; return false; }
      locked = true;
      if (!rec.ring || room + DIAG_REC_GAP_ROOM > DIAG_REC_BLOCK_SIZE - DIAG_REC_BLOCK_HDR) { release(); return false; }
      now = rec.env->millis();
      const uint32_t drops = rec.pendingDrops;
      rec.ensureRoom(room + (drops ? DIAG_REC_GAP_ROOM : 0u), now);
      p = rec.cur() + rec.used;
      if (drops) {
        rec.pendingDrops = 0;
        rec.droppedTotal += drops;
        begin(DIAG_T_GAP);
        p += diagPutVarint(p, drops);
        finish();
      }
      return true;
    }

    void begin(uint8_t type) {
      *p++ = type;
      p += diagPutVarint(p, (uint32_t)(now - r->lastRecMs));
      r->lastRecMs = now;
    }
    void u8(uint8_t v) { *p++ = v; }
    void u16(uint16_t v) { *p++ = (uint8_t)v; *p++ = (uint8_t)(v >> 8); }
    void i16(int16_t v) { u16((uint16_t)v); }
    void u32(uint32_t v) { u16((uint16_t)v); u16((uint16_t)(v >> 16)); }
    void text(const char* s, size_t n) {
      lastTextLen = p;
      if (n > DIAG_REC_TEXT_MAX) { *p++ = (uint8_t)DIAG_REC_TEXT_CUT; n = DIAG_REC_TEXT_MAX; }
      else { *p++ = (uint8_t)n; }
      if (n) { memcpy(p, s, n); p += n; }
    }
    // The caller clipped the text to 254 bytes and knows the original was longer.
    void markLastTextCut() { if (lastTextLen) *lastTextLen = (uint8_t)DIAG_REC_TEXT_CUT; }
    void finish() { r->used = (uint32_t)(p - r->cur()); r->recSeq++; }
    void close() { finish(); release(); }
    void release() { if (locked && r && r->env) r->env->unlock(); locked = false; }
  };
  friend struct Writer;

  uint8_t* cur() const { return ring + (size_t)(curSeq % nBlocks) * DIAG_REC_BLOCK_SIZE; }

  // Make sure `room` more bytes fit in the current block; open the next block when they do not.
  void ensureRoom(size_t room, uint32_t now) {
    if (haveBlock && used + room <= DIAG_REC_BLOCK_SIZE) return;
    curSeq = haveBlock ? curSeq + 1u : 0u;
    uint8_t* b = cur();
    memset(b, 0, DIAG_REC_BLOCK_SIZE);                  // type 0 ends the block
    putU32(b + 0, curSeq); putU32(b + 4, now); putU32(b + 8, env->epoch()); putU32(b + 12, recSeq);
    used = DIAG_REC_BLOCK_HDR;
    haveBlock = true;
    lastRecMs = now;                                    // the first record of a block has dt 0
    clearCaches();
  }
  static void putU32(uint8_t* p, uint32_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24); }
};

// ---- download and status (TASK-1666, ADR-213 decision 5) ----------------------
// The download is a file header, then the blocks [first, first + count). The range is fixed by
// diagDownloadRange() when the request starts; the filler diagDownloadFill() then serves it in the
// chunks the web server asks for. The ring is never copied: each fill copies at most one chunk
// (about one TCP window) under a try-lock and answers "try again" when the lock is busy.
//
// FILE HEADER (all integers little-endian), rebuilt by the glue for every request:
//    0  magic "OTGWREC\0"                     8 bytes
//    8  format version                        u16  (DIAG_REC_FORMAT_VERSION)
//   10  header length incl. this field        u16  (blocks start at this offset)
//   12  block size                            u32
//   16  first block_seq in this download      u32
//   20  block count in this download          u32  (0 = nothing newer than `since`, or no recording)
//   24  ring size in blocks                   u32
//   28  boot count                            u32
//   32  flags                                 u8   (bit 0: ring in PSRAM)
//   33  reserved                              3 bytes (0)
//   36  firmware version                      u8 len + text
//       firmware githash                      u8 len + text
//       route names                           u8 count, then per route (index = route id of a REST record) u8 len + text
// A block that was overwritten after the range was fixed is served as a "lost" block: block_seq
// as expected, first_record_seq = DIAG_REC_LOST_SEQ, the rest zero (type 0 ends the block), so the
// decoder sees an empty block where the gap is.
#define DIAG_REC_HDR_MAX          640u
#define DIAG_REC_LOST_SEQ         0xFFFFFFFFu
#define DIAG_FILL_TRY_AGAIN       ((size_t)0xFFFFFFFFu)     // == RESPONSE_TRY_AGAIN of the async web server

struct DiagDownload {
  uint32_t first = 0;                       // first block_seq served
  uint32_t count = 0;                       // blocks served (fixed at request start)
  uint16_t hdrLen = 0;                      // 0 until the header is complete
  uint8_t  hdr[DIAG_REC_HDR_MAX];
  size_t totalBytes() const { return (size_t)hdrLen + (size_t)count * DIAG_REC_BLOCK_SIZE; }
};

// Fix the range under the recorder lock. since: serve blocks with block_seq > since (hasSince), else
// from the oldest block. Returns false when the lock is busy (the caller answers 503); a recorder
// that never ran or is stopped is a valid empty range.
static inline bool diagDownloadRange(DiagRecorder& r, DiagDownload& d, bool hasSince, uint32_t since) {
  d.first = 0; d.count = 0;
  if (!r.env) return true;                                  // never started
  if (!r.env->tryLock()) return false;
  if (r.hasData()) {
    const uint32_t oldest = r.oldestSeq(), newest = r.newestSeq();
    uint32_t first = oldest;
    if (hasSince && since >= newest) {
      d.first = newest;                                     // nothing newer than `since`
    } else {
      if (hasSince && since + 1u > first) first = since + 1u; // DIAG-SINCE
      d.first = first;
      d.count = newest + 1u - first;                        // the newest block now, not at fill time
    }
  }
  r.env->unlock();
  return true;
}

struct DiagHdrWriter {
  uint8_t* p; size_t cap; size_t n; bool ok;
  explicit DiagHdrWriter(DiagDownload& d) : p(d.hdr), cap(sizeof(d.hdr)), n(0), ok(true) {}
  void u8(uint8_t v)   { if (n + 1 > cap) { ok = false; return; } p[n++] = v; }
  void u16(uint16_t v) { u8((uint8_t)v); u8((uint8_t)(v >> 8)); }
  void u32(uint32_t v) { u16((uint16_t)v); u16((uint16_t)(v >> 16)); }
  void str(const char* s) {                                 // u8 len + bytes, cut at 255
    size_t l = strlen(s); if (l > 255) l = 255;
    u8((uint8_t)l);
    if (n + l > cap) { ok = false; return; }
    memcpy(p + n, s, l); n += l;
  }
};

// Fixed part of the header; the caller appends the route names (u8 count, then str each) and calls
// diagDownloadHeaderEnd().
static inline void diagDownloadHeaderBegin(DiagHdrWriter& w, const DiagDownload& d, const DiagRecorder& r,
                                           uint32_t bootCount, bool psram, const char* version, const char* githash) {
  static const char magic[8] = { 'O','T','G','W','R','E','C','\0' };
  for (uint8_t i = 0; i < 8; i++) w.u8((uint8_t)magic[i]);
  w.u16(DIAG_REC_FORMAT_VERSION);
  w.u16(0);                                                 // header length, patched by diagDownloadHeaderEnd()
  w.u32(DIAG_REC_BLOCK_SIZE);
  w.u32(d.first);
  w.u32(d.count);
  w.u32(r.nBlocks);
  w.u32(bootCount);
  w.u8(psram ? 1u : 0u);
  w.u8(0); w.u8(0); w.u8(0);
  w.str(version);
  w.str(githash);
}

static inline bool diagDownloadHeaderEnd(DiagHdrWriter& w, DiagDownload& d) {
  if (!w.ok || w.n > 0xFFFFu) { d.hdrLen = 0; return false; }
  w.p[10] = (uint8_t)w.n; w.p[11] = (uint8_t)(w.n >> 8);
  d.hdrLen = (uint16_t)w.n;
  return true;
}

// One filler call: `index` bytes of the body are already sent. Returns the bytes written, 0 at the
// end, or DIAG_FILL_TRY_AGAIN when the recorder lock is busy (the server calls again with the same index).
static inline size_t diagDownloadFill(DiagRecorder& r, const DiagDownload& d, uint8_t* buf, size_t maxLen, size_t index) {
  const size_t total = d.totalBytes();
  if (index >= total || maxLen == 0) return 0; // DIAG-RANGE-END
  if (index < d.hdrLen) {
    size_t n = d.hdrLen - index; if (n > maxLen) n = maxLen;
    memcpy(buf, d.hdr + index, n);
    return n;
  }
  const size_t off = index - d.hdrLen;
  const uint32_t seq = d.first + (uint32_t)(off / DIAG_REC_BLOCK_SIZE);
  const size_t within = off % DIAG_REC_BLOCK_SIZE;
  size_t n = DIAG_REC_BLOCK_SIZE - within; if (n > maxLen) n = maxLen;
  if (!r.env || !r.env->tryLock()) return DIAG_FILL_TRY_AGAIN;
  const uint8_t* b = r.block(seq);                          // null: stopped, freed or overwritten since the range was fixed
  if (b) {
    memcpy(buf, b + within, n);
  } else {
    for (size_t k = 0; k < n; k++) {
      const size_t pos = within + k;
      uint8_t v = 0;
      if (pos < 4)                    v = (uint8_t)(seq >> (8u * pos));
      else if (pos >= 12 && pos < 16) v = (uint8_t)(DIAG_REC_LOST_SEQ >> (8u * (pos - 12)));
      buf[k] = v;
    }
  }
  r.env->unlock();
  return n;
}

// Status snapshot for /api/v2/diag/recording/status. Taken under the try-lock; when the lock is busy
// the fields are read without it (a status line, not a data path) and `exact` is false.
struct DiagStatus {
  bool active, hasData, psram, exact;
  uint32_t ringBlocks, ringBytes, oldest, newest, dropped;
  DiagStartResult lastStart;
};
static inline void diagStatusSnapshot(DiagRecorder& r, DiagStatus& s) {
  const bool locked = (r.env != nullptr) && r.env->tryLock();
  s.exact = locked || (r.env == nullptr);                   // never started: nothing to race with
  s.active = r.active;
  s.hasData = r.hasData();
  s.psram = r.memIsPsram;
  s.ringBlocks = r.ring ? r.nBlocks : 0u;
  s.ringBytes = r.ring ? r.ringBytes() : 0u;
  s.oldest = s.hasData ? r.oldestSeq() : 0u;
  s.newest = s.hasData ? r.newestSeq() : 0u;
  s.dropped = r.droppedTotal + r.pendingDrops;
  s.lastStart = r.lastStart;
  if (locked) r.env->unlock();
}

#endif // DIAGRECORDERSTUFF_H

/***************************************************************************
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program. If not, see <https://www.gnu.org/licenses/>.
*
****************************************************************************
*/
