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
  OUT_REFUSED,   // 400: refused by a rule or the unique index (a resend, or the gateway is blocked)
  OUT_AUTH,      // 401/403: the token is no longer valid; log in again
  OUT_DOWN       // no answer, a timeout, 404 from the tunnel, 429, 5xx: the service counts as unreachable
};

inline Outcome classify(int status) {
  if (status == 200) return OUT_OK;
  if (status == 400) return OUT_REFUSED;
  if (status == 401 || status == 403) return OUT_AUTH;
  return OUT_DOWN;
}

}  // namespace crec
