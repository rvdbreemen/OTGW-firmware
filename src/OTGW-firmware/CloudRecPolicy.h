/*
***************************************************************************
**  Program  : CloudRecPolicy.h
**  TASK-1971 (branch research/cloud-recording only): when the cloud recorder retries, and what an HTTP
**  status from the collection point means. No Arduino dependencies, so tests/test_cloudrec_policy.py
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

namespace crec {

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

// The send queue (TASK-1974): one slot on every board, a second one in PSRAM where there is PSRAM. A closed record
// takes a free slot; the upload task always sends the oldest (lowest seq) first, so records arrive in order.
struct Slot {
  uint8_t *buf;          // nullptr: this slot does not exist on this board
  size_t len;
  int64_t t0;
  uint64_t seq;
  volatile bool busy;
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

}  // namespace crec
