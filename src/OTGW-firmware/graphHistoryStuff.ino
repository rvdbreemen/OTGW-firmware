/*
***************************************************************************
**  Program  : graphHistoryStuff.ino
**  Version  : v2.0.0-alpha.473
**
**  Copyright (c) 2021-2026 Robert van den Breemen
**
**  Device-side history of the Monitor graph series (TASK-1296): sampling from
**  the OT state once per slot (30 s, or 120 s with PSRAM) and GET /api/v2/graph/history. The pure
**  ring, commit and JSON logic is in graphHistoryStuff.h (host-tested).
**
**  TERMS OF USE: GNU GPLv3. See bottom of file.
***************************************************************************
*/

static GraphHistRing  graphHistRing;
static GraphHistClock graphHistClock;
static PlatformMutex  graphHistMutex = nullptr;
// Small ring, static (ADR-053): 126 x 14 B = 1764 B in .bss on every board. It is the whole
// history on a board without PSRAM and stays unused on a PSRAM board (one binary for all).
static GraphHistSlot  graphHistSmallSlots[GRAPH_HIST_SMALL_SLOTS];

// PSRAM boards keep 24 h (726 slots of 120 s, 10164 B) in one block that is allocated here,
// once, and never freed. This is a deliberate exception to ADR-053: that rule protects internal
// DRAM from fragmentation, and a single never-freed PSRAM block does not touch internal DRAM
// (ADR-193). Without PSRAM, or if the allocation fails, the small static ring is used.
void graphHistoryInit()
{
  void* big = platformPsramAllocOnce(GRAPH_HIST_LARGE_SLOTS * sizeof(GraphHistSlot));
  if (big) {
    graphHistInit(graphHistRing, graphHistClock, (GraphHistSlot*)big, GRAPH_HIST_LARGE_SLOTS, GRAPH_HIST_LARGE_INTERVAL_S);
    DebugTf(PSTR("Graph history: %u slots of %u s in PSRAM\r\n"), (unsigned)graphHistRing.cap, (unsigned)graphHistRing.intervalS);
  } else {
    graphHistInit(graphHistRing, graphHistClock, graphHistSmallSlots, GRAPH_HIST_SMALL_SLOTS, GRAPH_HIST_SMALL_INTERVAL_S);
    DebugTf(PSTR("Graph history: %u slots of %u s in static RAM\r\n"), (unsigned)graphHistRing.cap, (unsigned)graphHistRing.intervalS);
  }
  if (graphHistMutex == nullptr) graphHistMutex = platformMutexCreate();
}

// A value counts as present only after the boiler has answered that MsgID.
static inline bool graphHistSeen(uint8_t msgId) { return getMsgLastUpdated(msgId) != 0; }

// Called from doTaskEvery1s() on the loop task, the task that runs processOT(),
// so OTcurrentSystemState is read without a lock. Only the ring write and the
// clock update take the mutex (the REST handler reads them on async_tcp).
void graphHistoryTick()
{
  uint8_t f = 0;
  if (isFlameStatus())             f |= GRAPH_HIST_FLAG_FLAME;
  if (isDomesticHotWaterActive())  f |= GRAPH_HIST_FLAG_DHW;
  if (isCentralHeatingActive())    f |= GRAPH_HIST_FLAG_CH;
  graphHistClock.flagsAcc |= f;

  const uint32_t k = graphHistIntervalsDue(graphHistRing, graphHistClock, millis());
  if (k == 0) return;

  GraphHistSlot s = graphHistEmptySlot();
  s.flow    = graphHistEncTemp(OTcurrentSystemState.Tboiler, graphHistSeen(25));
  s.ret     = graphHistEncTemp(OTcurrentSystemState.Tret,    graphHistSeen(28));
  s.chSet   = graphHistEncTemp(OTcurrentSystemState.TSet,    graphHistSeen(1));
  s.roomSet = graphHistEncTemp(OTcurrentSystemState.TrSet,   graphHistSeen(16));
  s.room    = graphHistEncTemp(OTcurrentSystemState.Tr,      graphHistSeen(24));
  s.mod     = graphHistEncMod (OTcurrentSystemState.RelModLevel, graphHistSeen(17));
  // Outside follows the browser rule (v2.js): weather API when valid, else MsgID 27.
  if (state.sat.weather.bValid) s.outside = graphHistEncTemp(state.sat.weather.fTemperature, true);
  else                          s.outside = graphHistEncTemp(OTcurrentSystemState.Toutside, graphHistSeen(27));

  platformMutexLock(graphHistMutex);
  graphHistCommit(graphHistRing, graphHistClock, k, s);
  platformMutexUnlock(graphHistMutex);
}

// GET /api/v2/graph/history. Runs on async_tcp. head, count and the newest-slot
// age are frozen ONCE under the mutex into a per-response snapshot. The chunked
// emitter re-runs once per ~1460 B TCP window and every pass must produce the
// same bytes, so the closure reads only the snapshot and the slot data: it takes
// no lock and never reads millis() or the live head/count. The response stops
// GRAPH_HIST_MARGIN slots short of the ring size.
//
// The margin counts PUSHED SLOTS, not ticks. One late tick can push many gap slots
// at once (graphHistCommit), so a stream is only corrupted when the 1 Hz tick stalls
// for more than GRAPH_HIST_MARGIN intervals while a stream is in progress: 6 slots
// are 3 minutes on the small ring (30 s slots) and 12 minutes on the large ring (120 s
// slots). No current path does that: an ESP flash reboots the unit, and a PIC
// flash pauses the tick for about a minute. If it ever happened, the body would
// fail JSON.parse in the browser, which falls back to 'idle' and retries.
void sendGraphHistory()
{
  if (platformMaxFreeBlock() < 8192) { sendApiBusy(F("low heap")); return; }

  auto snap = std::make_shared<GraphHistSnapshot>();
  if (!platformMutexLock(graphHistMutex, 100)) { sendApiBusy(F("history busy")); return; }
  *snap = graphHistMakeSnapshot(graphHistRing, graphHistClock, millis());
  platformMutexUnlock(graphHistMutex);

  webPushHeader(F("Cache-Control"), F("no-cache"));
  restSendChunked("application/json", [snap](JsonEmit& je) {
    graphHistEmitJson(je, graphHistRing, *snap);
  });
}

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
