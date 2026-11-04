/*
***************************************************************************
**  Program  : SATboilerFeed.h
**  Version  : v2.0.0-beta.73
**
**  Copyright (c) 2021-2026 Robert van den Breemen
**
**  Validity and age of the boiler flow (MsgID 25) and return (MsgID 28)
**  temperature feeds that SAT reads (TASK-1412.5, Sergeant D entries 217
**  and 218). No Arduino or firmware state in here, so a host test compiles
**  this header as it is.
**
**  A feed remembers the millis() of its last valid sample (a boiler
**  Read-Ack) and of its last boiler reply of any kind. The class is worked
**  out from those two stamps, the verdict of the Unknown-DataID bitmap and
**  the validity window of the signal: the same stamps serve the PIC and the
**  OT-Direct path, because both feed processOT().
**
**  TERMS OF USE: GNU GPLv3. See OTGW-firmware.h for the full notice.
***************************************************************************
*/

#pragma once

#include <stdint.h>

// What the boiler last said about this MsgID.
enum class SatFeedEvent : uint8_t {
  None = 0,        // no boiler reply seen since boot
  Valid,           // Read-Ack: the value is a sample
  DataInvalid,     // Data-Invalid: the boiler knows the MsgID, the value is not available (OpenTherm v4.2 section 4.4.1)
  UnknownDataId    // Unknown-DataID: the boiler does not implement the MsgID
};

// What SAT may make of the feed right now.
enum class SatFeedClass : uint8_t {
  Unsupported,     // Unknown-DataID verdict (the last reply, or the persisted bitmap)
  NeverReceived,   // no boiler reply of any kind since boot
  DataInvalid,     // the last reply was Data-Invalid and the last valid sample is older than the window (or there is none)
  Stale,           // the last reply was a valid sample but it is older than the window: the link may be live, this signal is not
  FreshZero,       // a valid sample inside the window that reads exactly 0.0
  Valid            // a valid sample inside the window
};

struct SatBoilerFeed {
  uint32_t     sampleMs = 0;                 // millis() of the last valid sample; 0 = none since boot
  uint32_t     eventMs  = 0;                 // millis() of the last boiler reply of any kind; 0 = none since boot
  SatFeedEvent lastEvent = SatFeedEvent::None;
};

// 0 means "never", so a stamp taken at millis() == 0 becomes 1.
static inline uint32_t satFeedStamp(uint32_t nowMs)
{
  return nowMs ? nowMs : 1UL;
}

static inline void satFeedNote(SatBoilerFeed &f, SatFeedEvent e, uint32_t nowMs)
{
  if (e == SatFeedEvent::None) return;
  f.eventMs   = satFeedStamp(nowMs);
  f.lastEvent = e;
  if (e == SatFeedEvent::Valid) f.sampleMs = f.eventMs;
}

// The boiler reply to a read of this MsgID as a feed event. A frame the gateway made itself (an OT-Direct answer,
// loopback, the master-mode cache replay) or a replayed log line says nothing about the boiler.
static inline SatFeedEvent satFeedEventForFrame(bool boilerReply, bool madeByGatewayOrReplayed, bool readAck,
                                                bool dataInvalid, bool unknownDataId)
{
  if (!boilerReply || madeByGatewayOrReplayed) return SatFeedEvent::None;
  if (readAck)       return SatFeedEvent::Valid;
  if (dataInvalid)   return SatFeedEvent::DataInvalid;
  if (unknownDataId) return SatFeedEvent::UnknownDataId;
  return SatFeedEvent::None;
}

// What the boiler answered to SAT's MsgID 57 write (SH=), TASK-1412.13 (entry 252, QR-1). SAT keeps sending SH= by its normal rules
// whatever the answer; the verdict is only observed and published.
enum class SatShVerdict : uint8_t {
  Unknown = 0,     // no boiler answer to a MsgID 57 write since SAT's last SH= (or since SAT was switched off)
  Accepted,        // Write-Ack: the boiler took the value
  Rejected         // Data-Invalid or Unknown-DataID: the boiler refused or does not implement the write
};

// The boiler reply to a MsgID 57 write as a verdict; Unknown = this frame says nothing (keep the old verdict). A frame the gateway
// made itself or a replayed log line says nothing about the boiler, as for the feeds above.
static inline SatShVerdict satShVerdictForFrame(bool boilerReply, bool madeByGatewayOrReplayed, bool writeAck,
                                                bool dataInvalid, bool unknownDataId)
{
  if (!boilerReply || madeByGatewayOrReplayed) return SatShVerdict::Unknown;
  if (writeAck)                     return SatShVerdict::Accepted;
  if (dataInvalid || unknownDataId) return SatShVerdict::Rejected;
  return SatShVerdict::Unknown;
}

// Age of the last valid sample. No sample since boot counts as the time since boot (millis() starts at 0).
static inline uint32_t satFeedAgeMs(const SatBoilerFeed &f, uint32_t nowMs)
{
  return f.sampleMs ? (uint32_t)(nowMs - f.sampleMs) : nowMs;
}

static inline bool satFeedFresh(const SatBoilerFeed &f, uint32_t nowMs, uint32_t windowMs)
{
  return f.sampleMs != 0 && (uint32_t)(nowMs - f.sampleMs) < windowMs;
}

// unsupported: the Unknown-DataID bitmap says so (it survives a reboot, so it can hold before this boot saw a reply).
// value: the stored value, only to tell a fresh zero from a fresh reading.
static inline SatFeedClass satFeedClassify(const SatBoilerFeed &f, uint32_t nowMs, uint32_t windowMs, bool unsupported,
                                           float value)
{
  if (unsupported || f.lastEvent == SatFeedEvent::UnknownDataId) return SatFeedClass::Unsupported;
  if (satFeedFresh(f, nowMs, windowMs)) return value == 0.0f ? SatFeedClass::FreshZero : SatFeedClass::Valid;
  if (f.lastEvent == SatFeedEvent::None) return SatFeedClass::NeverReceived;
  if (f.lastEvent == SatFeedEvent::DataInvalid) return SatFeedClass::DataInvalid;
  return SatFeedClass::Stale;
}

static inline bool satFeedClassIsFresh(SatFeedClass c)
{
  return c == SatFeedClass::Valid || c == SatFeedClass::FreshZero;
}

// One letter per class for the SAT log line.
static inline char satFeedClassLetter(SatFeedClass c)
{
  switch (c) {
    case SatFeedClass::Unsupported:   return 'U';
    case SatFeedClass::NeverReceived: return 'N';
    case SatFeedClass::DataInvalid:   return 'I';
    case SatFeedClass::Stale:         return 'S';
    case SatFeedClass::FreshZero:     return 'Z';
    default:                          return 'V';
  }
}
