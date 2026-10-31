/*
***************************************************************************
**  Program  : graphHistoryStuff.h
**  Version  : v2.0.0-alpha.573
**
**  Copyright (c) 2021-2026 Robert van den Breemen
**
**  Device-side history of the Monitor graph series (TASK-1296).
**
**  The v2 Monitor graph used to hold only what the browser had collected
**  since the page was opened. This header is the pure, host-compilable part
**  of the fix: a ring of slots, the commit logic (gap fill, flag
**  accumulation), the value encoders and a template that writes the
**  GET /api/v2/graph/history body through any JsonEmit-compatible writer.
**  The glue (sampling from firmware state, mutex, REST handler, choice of
**  backing storage) lives in graphHistoryStuff.ino.
**
**  Two sizes, one slot layout (14 B):
**    - every board:  126 slots of 30 s in a static array (1764 B, ADR-053),
**                    120 emitted = 1 h;
**    - PSRAM boards: 726 slots of 120 s in one never-freed PSRAM block
**                    (10164 B), 720 emitted = 24 h.
**
**  Header naming: ADR-081. The component has a functional .ino, so its header
**  is <Component>stuff.h and holds the types as well.
**
**  TERMS OF USE: GNU GPLv3. See bottom of file.
***************************************************************************
*/

#ifndef GRAPHHISTORYSTUFF_H
#define GRAPHHISTORYSTUFF_H

#include <stdint.h>
#include <stddef.h>
#include <math.h>

#define GRAPH_HIST_MARGIN            6     // slots the response never reads (writer headroom)
#define GRAPH_HIST_SMALL_INTERVAL_S  30    // no PSRAM: one slot per 30 s
#define GRAPH_HIST_SMALL_SLOTS       126   // ring capacity; 120 emitted = 1 h
#define GRAPH_HIST_LARGE_INTERVAL_S  120   // PSRAM: one slot per 2 minutes
#define GRAPH_HIST_LARGE_SLOTS       726   // ring capacity; 720 emitted = 24 h
#define GRAPH_HIST_SCALE       10    // temperatures are stored and sent as value*10

#define GRAPH_HIST_FLAG_FLAME  0x01
#define GRAPH_HIST_FLAG_DHW    0x02
#define GRAPH_HIST_FLAG_CH     0x04

static const int16_t GRAPH_HIST_NO_TEMP = INT16_MIN;  // no value for a temperature column
static const uint8_t GRAPH_HIST_NO_MOD  = 255;        // no value for the modulation column

// One slot. Temperatures are value*10 rounded; mod is 0..100 rounded.
struct GraphHistSlot {
  int16_t flow;
  int16_t ret;
  int16_t chSet;
  int16_t roomSet;
  int16_t room;
  int16_t outside;
  uint8_t mod;
  uint8_t flags;
};
static_assert(sizeof(GraphHistSlot) == 14, "GraphHistSlot must stay 14 bytes");

inline GraphHistSlot graphHistEmptySlot() {
  GraphHistSlot s;
  s.flow = s.ret = s.chSet = s.roomSet = s.room = s.outside = GRAPH_HIST_NO_TEMP;
  s.mod = GRAPH_HIST_NO_MOD;
  s.flags = 0;
  return s;
}

struct GraphHistRing {
  GraphHistSlot* slot;   // backing storage, cap slots, owned by the caller
  uint16_t cap;          // ring capacity in slots
  uint16_t intervalS;    // seconds per slot
  uint16_t head;         // index the next commit writes to
  uint16_t count;        // valid slots, saturates at cap
  uint32_t total;        // commits ever made
};

// Sampling clock. slotStartMs advances by whole intervals, so jitter of the
// 1 s tick does not accumulate. lastCommitMs is the nominal time of the newest
// slot (the interval boundary it belongs to).
struct GraphHistClock {
  uint32_t slotStartMs;
  uint32_t lastCommitMs;
  uint8_t  flagsAcc;
  bool     started;
};

inline void graphHistInit(GraphHistRing& r, GraphHistClock& c, GraphHistSlot* slots, uint16_t cap, uint16_t intervalS) {
  r.slot = slots;
  r.cap = cap;
  r.intervalS = intervalS;
  r.head = 0;
  r.count = 0;
  r.total = 0;
  c.slotStartMs = 0;
  c.lastCommitMs = 0;
  c.flagsAcc = 0;
  c.started = false;
}

// ---- encoders --------------------------------------------------------------

// Temperature in degrees C to value*10 rounded. Absent, NaN and Inf give the
// no-value sentinel; out-of-range values clamp (INT16_MIN stays reserved).
inline int16_t graphHistEncTemp(float v, bool present) {
  if (!present || isnan(v) || isinf(v)) return GRAPH_HIST_NO_TEMP;
  float x = v * (float)GRAPH_HIST_SCALE;
  x = (x < 0.0f) ? (x - 0.5f) : (x + 0.5f);
  if (x > 32767.0f)  return 32767;
  if (x < -32767.0f) return -32767;
  return (int16_t)x;
}

// Relative modulation percent to 0..100 rounded. Absent or NaN gives 255.
inline uint8_t graphHistEncMod(float v, bool present) {
  if (!present || isnan(v) || isinf(v)) return GRAPH_HIST_NO_MOD;
  if (v <= 0.0f)   return 0;
  if (v >= 100.0f) return 100;
  return (uint8_t)(v + 0.5f);
}

// ---- ring ------------------------------------------------------------------

inline void graphHistPush(GraphHistRing& r, const GraphHistSlot& s) {
  r.slot[r.head] = s;
  r.head = (uint16_t)((r.head + 1) % r.cap);
  if (r.count < r.cap) r.count++;
  r.total++;
}

// Whole intervals elapsed since the current slot started; 0 = not due yet.
// The first call only starts the clock. A value above 1 means the tick ran
// late (for example during a firmware flash, when timers pause).
inline uint32_t graphHistIntervalsDue(const GraphHistRing& r, GraphHistClock& c, uint32_t nowMs) {
  if (!c.started) {
    c.started = true;
    c.slotStartMs = nowMs;
    return 0;
  }
  const uint32_t intervalMs = (uint32_t)r.intervalS * 1000UL;
  const uint32_t elapsed = nowMs - c.slotStartMs;
  return elapsed / intervalMs;
}

// Commit k intervals: k-1 empty slots for the intervals nobody sampled
// (capped at the ring size), then the real slot with the accumulated flags.
// The caller holds the ring mutex.
inline void graphHistCommit(GraphHistRing& r, GraphHistClock& c, uint32_t k, GraphHistSlot values) {
  if (k == 0) return;
  uint32_t gaps = k - 1;
  if (gaps > r.cap) gaps = r.cap;
  const GraphHistSlot empty = graphHistEmptySlot();
  for (uint32_t i = 0; i < gaps; i++) graphHistPush(r, empty);
  values.flags = c.flagsAcc;
  graphHistPush(r, values);
  c.flagsAcc = 0;
  c.slotStartMs += k * (uint32_t)r.intervalS * 1000UL;
  c.lastCommitMs = c.slotStartMs;
}

// ---- response snapshot -----------------------------------------------------

// Everything the JSON emitter needs that is not the (stable) slot data. Taken
// once per response under the ring mutex; the emitter never reads the live
// head, count, clock or millis().
struct GraphHistSnapshot {
  uint32_t newestAgeMs;   // millis() at snapshot time minus the newest slot time; 0 if no slot
  uint16_t start;         // ring index of the oldest emitted slot
  uint16_t n;             // emitted slots, at most cap - GRAPH_HIST_MARGIN
};

inline GraphHistSnapshot graphHistMakeSnapshot(const GraphHistRing& r, const GraphHistClock& c, uint32_t nowMs) {
  GraphHistSnapshot s;
  const uint16_t emitMax = (uint16_t)(r.cap - GRAPH_HIST_MARGIN);
  s.n = (r.count < emitMax) ? r.count : emitMax;
  s.start = (uint16_t)((r.head + r.cap - s.n) % r.cap);
  s.newestAgeMs = (r.total == 0) ? 0 : (nowMs - c.lastCommitMs);
  return s;
}

// ---- JSON ------------------------------------------------------------------

// Decimal text of v into buf (at least 7 bytes); returns buf.
inline const char* graphHistFmtInt(char* buf, int v) {
  char tmp[8];
  int n = 0;
  unsigned u = (v < 0) ? (unsigned)(-v) : (unsigned)v;
  do { tmp[n++] = (char)('0' + (u % 10)); u /= 10; } while (u && n < 7);
  int o = 0;
  if (v < 0) buf[o++] = '-';
  while (n) buf[o++] = tmp[--n];
  buf[o] = '\0';
  return buf;
}

template <class W>
void graphHistEmitTempColumn(W& je, const __FlashStringHelper* key, const GraphHistRing& r,
                             const GraphHistSnapshot& s, int16_t GraphHistSlot::*member) {
  char b[8];
  je.beginArray(key);
  for (uint16_t i = 0; i < s.n; i++) {
    const int16_t v = r.slot[(s.start + i) % r.cap].*member;
    if (v == GRAPH_HIST_NO_TEMP) je.raw("null");
    else                         je.raw(graphHistFmtInt(b, v));
  }
  je.endArray();
}

template <class W>
void graphHistEmitFlagColumn(W& je, const __FlashStringHelper* key, const GraphHistRing& r,
                             const GraphHistSnapshot& s, uint8_t mask) {
  je.beginArray(key);
  for (uint16_t i = 0; i < s.n; i++) {
    je.raw((r.slot[(s.start + i) % r.cap].flags & mask) ? "1" : "0");
  }
  je.endArray();
}

// Writes the whole response body: oldest slot first, columnar. Deterministic for
// a fixed snapshot as long as the writer does not push more than
// GRAPH_HIST_MARGIN slots during the stream (restSendChunked re-runs this
// per TCP window and every pass must be byte-identical). The margin counts
// pushed slots: a late tick pushes k-1 gap slots plus the real one in one go, so
// the stream is only at risk when the 1 Hz tick stalls for more than
// GRAPH_HIST_MARGIN intervals mid-stream (3 minutes on the 30 s ring, 12 minutes
// on the 120 s ring). No current path does
// (an ESP flash reboots, a PIC flash pauses for about a minute); a corrupted
// body would fail JSON.parse in the browser, which retries.
template <class W>
void graphHistEmitJson(W& je, const GraphHistRing& r, const GraphHistSnapshot& s) {
  char b[8];
  je.beginObject();
  je.field(F("interval_s"), (uint32_t)r.intervalS);
  je.field(F("newest_age_ms"), (uint32_t)s.newestAgeMs);
  je.field(F("count"), (uint32_t)s.n);
  je.field(F("scale"), (uint32_t)GRAPH_HIST_SCALE);
  graphHistEmitTempColumn(je, F("flow"),    r, s, &GraphHistSlot::flow);
  graphHistEmitTempColumn(je, F("ret"),     r, s, &GraphHistSlot::ret);
  graphHistEmitTempColumn(je, F("chSet"),   r, s, &GraphHistSlot::chSet);
  graphHistEmitTempColumn(je, F("roomSet"), r, s, &GraphHistSlot::roomSet);
  graphHistEmitTempColumn(je, F("room"),    r, s, &GraphHistSlot::room);
  graphHistEmitTempColumn(je, F("outside"), r, s, &GraphHistSlot::outside);
  je.beginArray(F("mod"));
  for (uint16_t i = 0; i < s.n; i++) {
    const uint8_t v = r.slot[(s.start + i) % r.cap].mod;
    if (v == GRAPH_HIST_NO_MOD) je.raw("null");
    else                        je.raw(graphHistFmtInt(b, v));
  }
  je.endArray();
  graphHistEmitFlagColumn(je, F("flame"),  r, s, GRAPH_HIST_FLAG_FLAME);
  graphHistEmitFlagColumn(je, F("dhw_on"), r, s, GRAPH_HIST_FLAG_DHW);
  graphHistEmitFlagColumn(je, F("ch"),     r, s, GRAPH_HIST_FLAG_CH);
  je.endObject();
}

// Implemented in graphHistoryStuff.ino.
void graphHistoryInit();    // create the ring mutex; call once from setup()
void graphHistoryTick();    // 1 Hz, loop task
void sendGraphHistory();    // GET /api/v2/graph/history

#endif // GRAPHHISTORYSTUFF_H

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
