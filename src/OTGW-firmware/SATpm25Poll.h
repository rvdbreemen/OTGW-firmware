/*
***************************************************************************
**  Program  : SATpm25Poll.h
**  Version  : v2.0.0-beta.246
**
**  Copyright (c) 2021-2026 Robert van den Breemen
**
**  SAT on a PIC board asks the boiler for the flow temperature (MsgID 25)
**  itself when the thermostat never reads it (TASK-1653, maintainer
**  decision entry 276). The command is PM=25 (otgw-6.6 gateway.asm
**  SetPrioMessage ~4984: RAM only). The PIC sends it only in an alternative
**  slot, i.e. in place of a thermostat request for a DataID it marks
**  unsupported (CreateRequest ~2386-2413, Alternative ~3390-3441); it never
**  adds a frame. No such slot, no answer.
**
**  This header holds the bus observations and the decision only. No Arduino
**  or firmware state in here, so a host test compiles it as it is.
**
**  TERMS OF USE: GNU GPLv3. See OTGW-firmware.h for the full notice.
***************************************************************************
*/

#pragma once

#include <stdint.h>
#include "SATboilerFeed.h"

// Robert 2026-11-05 (TASK-1653 notes).
static const uint32_t SAT_PM25_THERMOSTAT_QUIET_MS = 300000UL;   // (a) no thermostat read of MsgID 25 for 300 s
static const uint32_t SAT_PM25_FLOW_AGE_MS         = 20000UL;    // (b) the last valid flow sample is at least 20 s old (N)
static const uint32_t SAT_PM25_SLOT_WINDOW_MS      = 120000UL;   // (c) an alternative slot was seen in the last 120 s
static const uint32_t SAT_PM25_FAULT_HOLDOFF_MS    = 10000UL;    // no PM=25 for 10 s after a fault edge in MsgID 0
// A T frame and the R frame that replaces it are printed back to back; the same window as the
// gateway-substitution pairing in processOT().
static const uint32_t SAT_PM25_PAIR_GAP_MS         = 500UL;

// OpenTherm message types (spec v4.2 table 2), as OTGW-Core.h OTLibMessageType.
static const uint8_t SAT_PM25_OT_READ_DATA = 0;
static const uint8_t SAT_PM25_OT_READ_ACK  = 4;
static const uint8_t SAT_PM25_OT_UNKNOWN   = 7;   // Unknown-DataID
// The bits of the MsgID 0 slave status the PIC checks before it schedules its own fault-code priority read: fault (bit 0) and
// diagnostic (bit 6), gateway.asm statusreturn ~2751-2759 (andlw b'01000001').
static const uint8_t SAT_PM25_FAULT_MASK   = 0x41;

// What processOT() saw on the bus. Only processOT() writes it; millis() stamps, 0 = never since boot.
struct SatPm25Bus {
  uint32_t t25ReadMs   = 0;      // the last thermostat T frame Read-Data of MsgID 25
  uint32_t slotMs      = 0;      // the last alternative slot seen (satPm25NoteFrame)
  uint32_t faultEdgeMs = 0;      // the last rising edge of the fault/diagnostic bits in a boiler MsgID 0 reply
  bool     faultBits   = false;  // those bits in the last boiler MsgID 0 reply
};

// One incoming frame (before processOT()'s one-frame delay) with the frame before it. src is the PIC's source letter
// ('T', 'B', 'R', 'A'). The caller leaves out frames the gateway made itself and replayed lines, and passes havePrev=false
// when the previous frame was one of those. An R frame is never a thermostat read: SAT's own PM=25 shows on the bus as
// T(X) R(25) and must not count as the thermostat reading MsgID 25.
//
// An alternative slot is a thermostat request the PIC may replace. Two signatures count:
//  1. T for DataID X, then R for another DataID: the PIC replaced the request (a slot in use).
//  2. T for DataID X, then the boiler's B for X with Unknown-DataID: the thermostat polls a DataID the boiler rejects. The PIC
//     marks it unsupported after 3 such replies in a row (HandleResponse ~2436-2446, 2-bit count) and from then on uses that
//     request for an alternative (CreateRequest ~2396-2399 -> SendAltRequest). While nothing is pending (no init message, no
//     priority message, an empty AA list) the PIC sends the request unmodified (Alternative ~3447-3448 RestoreRequest ->
//     SendDefaultMsg ~2401-2403) and prints no R (R/A only through SetParity ~2492-2496), so signature 1 never shows then.
//     The AA list starts as 116-123 (AlternativeCmd ~5479), but the PIC deletes each entry the boiler rejects
//     (MsgUnsupported ~2446 DeleteAlt), so on a boiler without those counters it ends up empty.
//     Signature 2 is a widening of the rule in the TASK-1653 notes (which name only signature 1); Robert chose to keep both
//     signatures (DM 2026-11-12, answer A, msg 1570300530331357285).
static inline void satPm25NoteFrame(SatPm25Bus &b, char src, uint8_t type, uint8_t id, uint8_t valueLB,
                                    bool havePrev, char prevSrc, uint8_t prevId, uint32_t gapMs, uint32_t nowMs)
{
  if (src == 'T' && type == SAT_PM25_OT_READ_DATA && id == 25) b.t25ReadMs = satFeedStamp(nowMs);
  const bool afterT = havePrev && prevSrc == 'T' && gapMs < SAT_PM25_PAIR_GAP_MS;
  if (afterT && src == 'R' && prevId != id) b.slotMs = satFeedStamp(nowMs);
  if (afterT && src == 'B' && prevId == id && type == SAT_PM25_OT_UNKNOWN) b.slotMs = satFeedStamp(nowMs);
  if (src == 'B' && id == 0 && type == SAT_PM25_OT_READ_ACK) {
    const bool fault = (valueLB & SAT_PM25_FAULT_MASK) != 0;
    if (fault && !b.faultBits) b.faultEdgeMs = satFeedStamp(nowMs);
    b.faultBits = fault;
  }
}

enum class SatPm25Verdict : uint8_t {
  Idle,     // nothing to do: a gate is closed, the thermostat reads MsgID 25 itself, or the flow sample is recent
  NoSlot,   // SAT needs MsgID 25 but no alternative slot was seen in the last 120 s: a PM=25 would never be answered
  Hold,     // SAT needs MsgID 25, but a fault edge is less than 10 s old (the PIC's own fault-code read has the slot)
  Send      // ask: PM=25
};

// gatesOk: PIC board in gateway mode, not PS=1, SAT enabled and controlling, MsgID 25 not marked unsupported, no simulation.
// flowAgeMs: satFeedAgeMs() of the flow feed (time since boot when there never was a valid sample).
static inline SatPm25Verdict satPm25Decide(const SatPm25Bus &b, uint32_t nowMs, uint32_t flowAgeMs, bool gatesOk)
{
  if (!gatesOk) return SatPm25Verdict::Idle;
  const uint32_t sinceT25 = b.t25ReadMs ? (uint32_t)(nowMs - b.t25ReadMs) : nowMs;   // never: time since boot
  if (sinceT25 < SAT_PM25_THERMOSTAT_QUIET_MS) return SatPm25Verdict::Idle;
  if (flowAgeMs < SAT_PM25_FLOW_AGE_MS) return SatPm25Verdict::Idle;
  if (b.slotMs == 0 || (uint32_t)(nowMs - b.slotMs) >= SAT_PM25_SLOT_WINDOW_MS) return SatPm25Verdict::NoSlot;
  if (b.faultEdgeMs != 0 && (uint32_t)(nowMs - b.faultEdgeMs) < SAT_PM25_FAULT_HOLDOFF_MS) return SatPm25Verdict::Hold;
  return SatPm25Verdict::Send;
}

// TASK-1759 part C (Sergeant D, corrections entry 296, maintainer agreement relayed): on a PIC board SAT asks for the return
// temperature (MsgID 28) itself with PM=28 every 30 s, not with AA=28/DA=28. Our interpretation in entry 296 (open for
// correction): only while no valid MsgID 28 sample came for 30 s, only while SAT is on, not while the boiler answers MsgID 28
// Unknown-DataID. There is no thermostat-quiet rule for 28: a thermostat that reads MsgID 28 keeps the sample fresh, so SAT then
// asks nothing. PM=28 uses the same alternative slot and fault hold-off as PM=25, so it inherits signature 2 above.
static const uint32_t SAT_PM28_RETURN_AGE_MS = 30000UL;   // the last valid return sample is at least 30 s old

// gatesOk as for satPm25Decide(), with MsgID 28 not marked unsupported. retAgeMs: satFeedAgeMs() of the return feed.
static inline SatPm25Verdict satPm28Decide(const SatPm25Bus &b, uint32_t nowMs, uint32_t retAgeMs, bool gatesOk)
{
  if (!gatesOk) return SatPm25Verdict::Idle;
  if (retAgeMs < SAT_PM28_RETURN_AGE_MS) return SatPm25Verdict::Idle;
  if (b.slotMs == 0 || (uint32_t)(nowMs - b.slotMs) >= SAT_PM25_SLOT_WINDOW_MS) return SatPm25Verdict::NoSlot;
  if (b.faultEdgeMs != 0 && (uint32_t)(nowMs - b.faultEdgeMs) < SAT_PM25_FAULT_HOLDOFF_MS) return SatPm25Verdict::Hold;
  return SatPm25Verdict::Send;
}
