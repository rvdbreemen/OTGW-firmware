/*
***************************************************************************
**  Program  : tcpRingDiag.h
**
**  TASK-3119 follow-up: a RAM ring with one lwIP / heap sample per second, so a stall after a memory emergency can be read
**  afterwards (bench 2026-11-27: :80 and :23 silent for 17 s to 2.6 min after the K hold, cause not known). Everything in this
**  file exists only when _VERSION_PRERELEASE is defined; a stable build compiles to nothing (host test tests/test_tcp_ring_diag.py).
**
**  Pure helpers only (no globals of the firmware): the sample, the ring, the armed rule. The firmware side is
**  tcpRingTick() / tcpRingRequestDump() in helperStuff.ino and the telnet key 'T' in handleDebug.ino.
***************************************************************************
*/
#pragma once

#if defined(_VERSION_PRERELEASE)

#include <stdint.h>

// One sample per second, 12 bytes: free heap and largest free block in 16-byte units (a floor of 1.6 KB reads exactly), the lwIP pcb
// counts clamped to 255. 240 samples = 2880 B of RAM (prerelease only) and 4 minutes of history: a 2 minute K hold plus its stall.
struct TcpRingSample {
  uint32_t upSec;      // state.uptime.iSeconds when the sample was taken
  uint16_t freeQ;      // free heap / 16, clamped to 65535
  uint16_t maxBlockQ;  // largest free block / 16, clamped to 65535
  uint8_t  active;     // active TCP pcbs
  uint8_t  timeWait;   // TIME_WAIT pcbs
  uint8_t  tx80;       // pcbs on local port 80 with unsent or unacked data
  uint8_t  reserved;
};
static_assert(sizeof(TcpRingSample) == 12, "TcpRingSample must stay 12 bytes");

static const uint16_t TCP_RING_N = 240;
static const uint32_t TCP_RING_ARM_BELOW = 8192;   // a sample under this free heap marks the ring worth reading

struct TcpRing {
  TcpRingSample s[TCP_RING_N];
  uint16_t head  = 0;   // index of the next write
  uint16_t count = 0;   // samples held, at most TCP_RING_N
};

static inline uint16_t tcpRingQ(uint32_t bytes) { uint32_t q = bytes / 16u; return (uint16_t)(q > 65535u ? 65535u : q); }
static inline uint8_t  tcpRingClamp8(uint32_t v) { return (uint8_t)(v > 255u ? 255u : v); }

static inline void tcpRingPush(TcpRing& r, uint32_t upSec, uint32_t freeHeap, uint32_t maxBlock,
                               uint32_t active, uint32_t timeWait, uint32_t tx80) {
  TcpRingSample& x = r.s[r.head];
  x.upSec = upSec; x.freeQ = tcpRingQ(freeHeap); x.maxBlockQ = tcpRingQ(maxBlock);
  x.active = tcpRingClamp8(active); x.timeWait = tcpRingClamp8(timeWait); x.tx80 = tcpRingClamp8(tx80); x.reserved = 0;
  r.head = (uint16_t)((r.head + 1u) % TCP_RING_N);
  if (r.count < TCP_RING_N) r.count++;
}

// i = 0 is the oldest sample held, i = count - 1 the newest.
static inline const TcpRingSample& tcpRingAt(const TcpRing& r, uint16_t i) {
  const uint16_t start = (uint16_t)((r.head + TCP_RING_N - r.count) % TCP_RING_N);
  return r.s[(start + i) % TCP_RING_N];
}

// Lowest free heap held in the ring, in bytes (16-byte resolution); 0xFFFFFFFF when the ring is empty.
static inline uint32_t tcpRingMinFree(const TcpRing& r) {
  uint32_t m = 0xFFFFFFFFu;
  for (uint16_t i = 0; i < r.count; i++) { const uint32_t f = (uint32_t)tcpRingAt(r, i).freeQ * 16u; if (f < m) m = f; }
  return m;
}

static inline bool tcpRingArmed(const TcpRing& r) { return r.count > 0 && tcpRingMinFree(r) < TCP_RING_ARM_BELOW; }

void tcpRingRequestDump();   // helperStuff.ino: the loop task prints the ring to telnet at its next tick (telnet key 'T')

#endif  // _VERSION_PRERELEASE
