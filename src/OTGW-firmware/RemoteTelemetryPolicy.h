/*
***************************************************************************
**  Program  : RemoteTelemetryPolicy.h
**  TASK-2903 (branch research/cloud-recording only): when the remote telemetry recorder retries, and what an HTTP
**  status from the collection point means. No Arduino dependencies, so tests/test_remote_telemetry_policy.py
**  compiles this header as it is.
**
**  Backoff while the service is unreachable: 5 min after the first failure, doubling per failure, capped at
**  24 h; each wait is stretched by a random 0 to 20 % so that gateways that lost the service at the same
**  moment do not all come back in the same minute. Base waits: 5, 10, 20, 40, 80, 160, 320, 640, 1280 min,
**  then 24 h for every failure after that. Reset to step 0 on the first success.
***************************************************************************
*/
#pragma once

#include <stddef.h>
#include <stdint.h>

namespace rtel {

static const uint32_t BACKOFF_FIRST_MS = 5UL * 60UL * 1000UL;
static const uint32_t BACKOFF_MAX_MS   = 24UL * 60UL * 60UL * 1000UL;
static const uint8_t  BACKOFF_STEPS    = 10;   // step 9 and later are capped at 24 h

// Wait before the next attempt after failure number step + 1. rnd is any 32-bit random value.
inline uint32_t backoffMs(uint8_t step, uint32_t rnd) {
  uint64_t base = (uint64_t)BACKOFF_FIRST_MS << (step < BACKOFF_STEPS ? step : BACKOFF_STEPS);
  if (base > BACKOFF_MAX_MS) base = BACKOFF_MAX_MS;
  uint64_t wait = base + base * (rnd % 2001u) / 10000u;    // + 0 to 20.00 %
  return wait > BACKOFF_MAX_MS ? BACKOFF_MAX_MS : (uint32_t)wait;
}

enum Outcome : uint8_t {
  OUT_OK,        // 200: stored
  OUT_REFUSED,   // 400: refused by a rule or the unique index (a resend of a stored seq)
  OUT_AUTH,      // 401/403: the token is no longer valid; log in again
  OUT_DOWN,      // no answer, a timeout, 404 from the tunnel, 429, 5xx: the service counts as unreachable
  OUT_BLOCKED    // 403 whose body holds "otgw-blocked" (pb_hooks/blocked.pb.js): turn the opt-in off, stop
};

inline Outcome classify(int status) {
  if (status == 200) return OUT_OK;
  if (status == 400) return OUT_REFUSED;
  if (status == 401 || status == 403) return OUT_AUTH;
  return OUT_DOWN;
}

// classify() plus the reply body: the collection point marks a blocked gateway in its 403 message.
inline Outcome classifyReply(int status, const char *body) {
  if (status == 403 && body) {
    for (const char *p = body; *p; p++)
      if (p[0] == 'o' && !__builtin_strncmp(p, "otgw-blocked", 12)) return OUT_BLOCKED;
  }
  return classify(status);
}

// The send queue (TASK-2905): one slot on every board, a second one in PSRAM where there is PSRAM. A closed record
// takes a free slot; the upload task always sends the oldest (lowest seq) first, so records arrive in order.
struct Slot {
  uint8_t *buf;          // nullptr: this slot does not exist on this board
  size_t len;
  int64_t t0;
  uint64_t seq;
  volatile bool busy;
  bool stored;           // TASK-2907: already written to the local ring
};

inline Slot *slotFree(Slot *s, int n) {
  for (int i = 0; i < n; i++) if (s[i].buf && !s[i].busy) return &s[i];
  return nullptr;
}

inline Slot *slotOldest(Slot *s, int n) {
  Slot *o = nullptr;
  for (int i = 0; i < n; i++) if (s[i].busy && (!o || s[i].seq < o->seq)) o = &s[i];
  return o;
}

// ---------------------------------------------------------------- TASK-2906: telemetry mode set centrally
//
// The maintainer sets mode (default | full | full+trace | custom), flags (for custom) and mode_until in the
// gateway's device record; the gateway reads them from every login and token-refresh reply. The ten log flags of
// debugFlags.h can be switched this way, never the simulation switches. full = every log flag but sattrace (high
// volume); full+trace = all ten; custom = the chosen flags; default, an empty mode, or a mode past mode_until = none.

static const char *const REMOTE_FLAGS[] = {"otmsg", "restapi", "mqtt", "sensors", "sat", "otdirect", "satble",
                                           "sattrace", "mqttgate", "ntp"};
static const int REMOTE_FLAG_COUNT = 10;
static const uint16_t REMOTE_ALL = (1u << REMOTE_FLAG_COUNT) - 1;
static const uint16_t REMOTE_TRACE = 1u << 7;   // sattrace

enum RemoteMode : uint8_t { RM_DEFAULT, RM_FULL, RM_FULL_TRACE, RM_CUSTOM };

struct RemoteCfg {
  RemoteMode mode;
  uint16_t flags;        // custom only, bit i = REMOTE_FLAGS[i]
  int64_t untilEpoch;    // 0 = no end
};

inline bool remoteStrEq(const char *a, size_t n, const char *b) {
  size_t k = 0;
  while (k < n && b[k] && a[k] == b[k]) k++;
  return k == n && !b[k];
}

// Finds "key":" in json and returns the start of the value (after the quote) and its length; false if absent.
inline bool remoteStrField(const char *json, const char *key, const char *&val, size_t &len) {
  size_t kl = 0;
  while (key[kl]) kl++;
  for (const char *p = json; *p; p++) {
    if (p[0] != '"') continue;
    size_t i = 0;
    while (i < kl && p[1 + i] == key[i]) i++;
    if (i == kl && p[1 + kl] == '"' && p[2 + kl] == ':' && p[3 + kl] == '"') {
      val = p + 4 + kl;
      len = 0;
      while (val[len] && val[len] != '"') len++;
      return true;
    }
  }
  return false;
}

// Days since 1970-01-01 of a civil date (proleptic Gregorian).
inline int64_t remoteDays(int64_t y, int64_t m, int64_t d) {
  y -= m <= 2;
  int64_t era = (y >= 0 ? y : y - 399) / 400;
  int64_t yoe = y - era * 400;
  int64_t doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  int64_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + doe - 719468;
}

// "YYYY-MM-DD HH:MM:SS[.sss]Z" (PocketBase date) to epoch seconds; false when it is not that form.
inline bool remoteParseDate(const char *s, size_t n, int64_t &epoch) {
  if (n < 19) return false;
  int v[6];
  const int at[6] = {0, 5, 8, 11, 14, 17}, w[6] = {4, 2, 2, 2, 2, 2};
  for (int f = 0; f < 6; f++) {
    v[f] = 0;
    for (int k = 0; k < w[f]; k++) {
      char c = s[at[f] + k];
      if (c < '0' || c > '9') return false;
      v[f] = v[f] * 10 + (c - '0');
    }
  }
  if (s[4] != '-' || s[7] != '-' || (s[10] != ' ' && s[10] != 'T') || s[13] != ':' || s[16] != ':') return false;
  epoch = remoteDays(v[0], v[1], v[2]) * 86400 + v[3] * 3600 + v[4] * 60 + v[5];
  return true;
}

// Reads mode, flags and mode_until from the "record" object of a login or refresh reply.
inline RemoteCfg parseRemote(const char *reply) {
  RemoteCfg c{RM_DEFAULT, 0, 0};
  const char *rec = reply;
  for (const char *p = reply; *p; p++)
    if (!__builtin_strncmp(p, "\"record\"", 8)) { rec = p; break; }
  const char *v;
  size_t n;
  if (remoteStrField(rec, "mode", v, n)) {
    if (remoteStrEq(v, n, "full")) c.mode = RM_FULL;
    else if (remoteStrEq(v, n, "full+trace")) c.mode = RM_FULL_TRACE;
    else if (remoteStrEq(v, n, "custom")) c.mode = RM_CUSTOM;
  }
  for (const char *p = rec; *p; p++) {
    if (__builtin_strncmp(p, "\"flags\":[", 9)) continue;
    for (const char *q = p + 9; *q && *q != ']'; q++) {
      if (*q != '"') continue;
      const char *e = q + 1;
      while (*e && *e != '"') e++;
      for (int i = 0; i < REMOTE_FLAG_COUNT; i++)
        if (remoteStrEq(q + 1, (size_t)(e - q - 1), REMOTE_FLAGS[i])) c.flags |= (uint16_t)(1u << i);
      q = e;
      if (!*q) break;
    }
    break;
  }
  if (remoteStrField(rec, "mode_until", v, n) && n) {
    int64_t t;
    if (remoteParseDate(v, n, t)) c.untilEpoch = t;
  }
  return c;
}

// The log flags this mode switches on at nowEpoch.
inline uint16_t remoteEffective(const RemoteCfg &c, int64_t nowEpoch) {
  if (c.untilEpoch && nowEpoch >= c.untilEpoch) return 0;
  switch (c.mode) {
    case RM_FULL: return (uint16_t)(REMOTE_ALL & ~REMOTE_TRACE);
    case RM_FULL_TRACE: return REMOTE_ALL;
    case RM_CUSTOM: return (uint16_t)(c.flags & REMOTE_ALL);
    default: return 0;
  }
}

}  // namespace rtel
