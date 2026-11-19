/*
***************************************************************************
**  Program  : graphHistoryStuff.h
**  Version  : v2.0.0-beta.379
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
**    - PSRAM boards: 5046 slots of 120 s in one never-freed PSRAM block
**                    (70644 B), up to 5040 emitted = 7 days (TASK-1963,
**                    ADR-245). A response holds at most 1008 slots: 24 h by
**                    default (720 x 2 min); ?hours= up to 7 d is thinned to
**                    longer slots (7 d = 1008 x 10 min).
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
#define GRAPH_HIST_LARGE_SLOTS       5046  // ring capacity; 5040 = 7 d (TASK-1963, ADR-245)
#define GRAPH_HIST_DEFAULT_WINDOW_S  86400 // a response without ?hours= covers at most 24 h
#define GRAPH_HIST_MAX_WINDOW_S      604800 // ?hours= is capped at 7 d
#define GRAPH_HIST_MAX_EMIT          1008  // slots per response: 7 d at 10 min; a longer window is thinned
// TASK-1937: a weather value older than this is stale for the ring, the same 1 h rule as SAT_OUTSIDE_VALID_MS
// (SATcontrol.ino, Sergeant D entry 57). tests/test_graph_outside_source.py checks both stay equal.
#define GRAPH_HIST_WEATHER_VALID_MS  3600000UL
#define GRAPH_HIST_SCALE       10    // temperatures are stored and sent as value*10

#define GRAPH_HIST_FLAG_FLAME  0x01
#define GRAPH_HIST_FLAG_DHW    0x02
#define GRAPH_HIST_FLAG_CH     0x04

static const int16_t GRAPH_HIST_NO_TEMP = INT16_MIN;  // no value for a temperature column
static const uint8_t GRAPH_HIST_NO_MOD  = 255;        // no value for the modulation column

// One slot. Temperatures are value*10 rounded; mod is 0..100 rounded.
// TASK-2018 (ADR-256, amends ADR-193/245): satTarget is the SAT target while SAT is enabled, else no value.
struct GraphHistSlot {
  int16_t flow;
  int16_t ret;
  int16_t chSet;
  int16_t roomSet;
  int16_t room;
  int16_t outside;
  int16_t satTarget;
  uint8_t mod;
  uint8_t flags;
};
static_assert(sizeof(GraphHistSlot) == 16, "GraphHistSlot must stay 16 bytes (ADR-256)");

inline GraphHistSlot graphHistEmptySlot() {
  GraphHistSlot s;
  s.flow = s.ret = s.chSet = s.roomSet = s.room = s.outside = s.satTarget = GRAPH_HIST_NO_TEMP;
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

// Room temperature dropout (TASK-1578). Same rule as the live graph (v2.js graphRoomLost, TASK-1546):
// the room is lost when SAT is on and reports room_temp_lost, or when MsgID 24 has been silent for
// more than GRAPH_HIST_ROOM_STALE_S (SAT's own room timeout, about 4.5 min). A unit that never
// reported MsgID 24 has no dropout: its room stays "no value" as before.
#define GRAPH_HIST_ROOM_STALE_S 270

inline bool graphHistRoomLost(bool satRoomLost, bool seen, uint32_t ageS) {
  return satRoomLost || (seen && ageS > GRAPH_HIST_ROOM_STALE_S);
}

// Room column of a slot: the reading while it is current, the no-value sentinel during a dropout.
inline int16_t graphHistEncRoom(float v, bool seen, bool satRoomLost, uint32_t ageS) {
  return graphHistEncTemp(v, seen && !graphHistRoomLost(satRoomLost, seen, ageS));
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
  uint16_t n;             // emitted (group) slots, at most GRAPH_HIST_MAX_EMIT
  uint16_t step;          // ring slots per emitted slot (1 = full resolution)
  bool     hasSatTarget;  // a slot of the window holds a SAT target: emit the satTarget array (ADR-256)
};

// TASK-1963 (ADR-245): windowS is the span the response covers (default 24 h, capped at 7 d and at
// what the ring holds, cap - GRAPH_HIST_MARGIN). The restSendChunked emitter re-runs the whole body
// for every TCP window, so its cost grows with the square of the body; a window of more than
// GRAPH_HIST_MAX_EMIT ring slots is therefore served as groups of `step` slots, aligned to the
// newest slot. step 1 gives the same body as before.
inline GraphHistSnapshot graphHistMakeSnapshot(const GraphHistRing& r, const GraphHistClock& c, uint32_t nowMs,
                                               uint32_t windowS = GRAPH_HIST_DEFAULT_WINDOW_S) {
  GraphHistSnapshot s;
  const uint16_t emitMax = (uint16_t)(r.cap - GRAPH_HIST_MARGIN);
  uint32_t slots = (r.count < emitMax) ? r.count : emitMax;
  if (windowS > GRAPH_HIST_MAX_WINDOW_S) windowS = GRAPH_HIST_MAX_WINDOW_S;
  uint32_t want = (r.intervalS == 0) ? slots : windowS / r.intervalS;
  if (want < 1) want = 1;
  if (slots > want) slots = want;
  s.step = (uint16_t)((slots + GRAPH_HIST_MAX_EMIT - 1) / GRAPH_HIST_MAX_EMIT);
  if (s.step < 1) s.step = 1;
  s.n = (uint16_t)(slots / s.step);
  s.start = (uint16_t)((r.head + r.cap - (uint32_t)s.n * s.step) % r.cap);
  s.newestAgeMs = (r.total == 0) ? 0 : (nowMs - c.lastCommitMs);
  // TASK-2018 (ADR-256): decided once here, under the ring mutex, so every re-run of the emitter for a TCP window
  // gives the same body. The scan covers exactly the slots of the emitted groups.
  s.hasSatTarget = false;
  for (uint32_t k = 0; k < (uint32_t)s.n * s.step && !s.hasSatTarget; k++)
    s.hasSatTarget = r.slot[(s.start + k) % r.cap].satTarget != GRAPH_HIST_NO_TEMP;
  return s;
}

// Ring index of the newest ring slot of emitted group g.
inline uint16_t graphHistGroupNewest(const GraphHistRing& r, const GraphHistSnapshot& s, uint16_t g) {
  return (uint16_t)((s.start + (uint32_t)g * s.step + s.step - 1) % r.cap);
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
    // A group shows its newest value (the last one that is not empty), as one 2 min slot shows its last sample.
    int16_t v = GRAPH_HIST_NO_TEMP;
    for (uint16_t k = 0; k < s.step && v == GRAPH_HIST_NO_TEMP; k++)
      v = r.slot[(graphHistGroupNewest(r, s, i) + r.cap - k) % r.cap].*member;
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
    // A flag is set for a group when it was set in any of its slots, so a short burn stays visible.
    uint8_t f = 0;
    for (uint16_t k = 0; k < s.step; k++) f |= r.slot[(s.start + (uint32_t)i * s.step + k) % r.cap].flags;
    je.raw((f & mask) ? "1" : "0");
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
  je.field(F("interval_s"), (uint32_t)r.intervalS * s.step);
  je.field(F("newest_age_ms"), (uint32_t)s.newestAgeMs);
  je.field(F("count"), (uint32_t)s.n);
  je.field(F("scale"), (uint32_t)GRAPH_HIST_SCALE);
  graphHistEmitTempColumn(je, F("flow"),    r, s, &GraphHistSlot::flow);
  graphHistEmitTempColumn(je, F("ret"),     r, s, &GraphHistSlot::ret);
  graphHistEmitTempColumn(je, F("chSet"),   r, s, &GraphHistSlot::chSet);
  graphHistEmitTempColumn(je, F("roomSet"), r, s, &GraphHistSlot::roomSet);
  graphHistEmitTempColumn(je, F("room"),    r, s, &GraphHistSlot::room);
  graphHistEmitTempColumn(je, F("outside"), r, s, &GraphHistSlot::outside);
  // TASK-2018 (ADR-256): only when the window holds a SAT target, so a window without SAT keeps its body.
  if (s.hasSatTarget) graphHistEmitTempColumn(je, F("satTarget"), r, s, &GraphHistSlot::satTarget);
  je.beginArray(F("mod"));
  for (uint16_t i = 0; i < s.n; i++) {
    uint8_t v = GRAPH_HIST_NO_MOD;
    for (uint16_t k = 0; k < s.step && v == GRAPH_HIST_NO_MOD; k++)
      v = r.slot[(graphHistGroupNewest(r, s, i) + r.cap - k) % r.cap].mod;
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
