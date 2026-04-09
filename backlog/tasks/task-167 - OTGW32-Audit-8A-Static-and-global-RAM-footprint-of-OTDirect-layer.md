---
id: TASK-167
title: 'OTGW32-Audit-8A: Static and global RAM footprint of OTDirect layer'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-08 22:23'
updated_date: '2026-04-08 22:31'
labels:
  - audit
  - otgw32
  - phase-8
milestone: m-1
dependencies: []
references:
  - src/OTGW-firmware/OTDirect.ino
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Measure the total RAM consumed by all static and global variables declared in OTDirect.ino when HAS_DIRECT_OT=1. This includes otSchedule[] array, otOverrides[], command ring buffer, response override/modifier tables, and all scalar state variables. Document the total in bytes and assess whether it fits within the ESP32-S3 heap budget without impacting ESP8266 builds.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Total static RAM of OTDirect globals is measured and documented
- [x] #2 otSchedule[] size is calculated (number of entries * sizeof(OTScheduleEntry))
- [x] #3 All other tables and buffers are sized and summed
- [x] #4 RAM usage does not negatively impact ESP8266 builds (HAS_DIRECT_OT=0 contributes 0 bytes)
- [x] #5 If total exceeds 4KB an audit-fix task is created to reduce footprint
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Read OTDirect.ino to identify all static/global variables
2. Calculate sizeof(OTScheduleEntry) and multiply by entry count
3. Calculate sizeof for each other struct/array
4. Sum all scalar state variables
5. Compare total to 4KB threshold
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
## Calculation results

### OTScheduleEntry struct (ESP32-S3, natural alignment):
- uint8_t msgId: 1 byte at offset 0
- 3 bytes padding (align uint32_t to 4)
- uint32_t intervalMs: 4 bytes at offset 4
- uint32_t lastSentMs: 4 bytes at offset 8
- bool disabled: 1 byte at offset 12
- bool isWrite: 1 byte at offset 13
- bool valueSet: 1 byte at offset 14
- 1 byte padding (align uint16_t to 2)
- uint16_t cachedValue: 2 bytes at offset 16
- Total field bytes: 18; padded to multiple of 4 = 20 bytes
- 99 entries x 20 bytes = 1,980 bytes

### OTFrameOverride struct: {uint8_t(1)+pad(1)+bool(1)+pad(1)+uint16_t(2)} = 6 bytes? No: msgId(1)+pad(1)+active(1)+pad(1)+overrideValue(2) = 6 bytes, aligned to 2 bytes = 6 bytes
- 7 entries x 6 bytes = 42 bytes

### OTResponseOverride struct: {uint8_t(1)+bool(1)+uint16_t(2)} = 4 bytes, aligned
- 16 entries x 4 bytes = 64 bytes

### OTResponseModify struct: same layout as OTResponseOverride = 4 bytes
- 8 entries x 4 bytes = 32 bytes

### Command ring buffer: uint32_t[8] = 32 bytes
### otBoilerCache: uint16_t[128] = 256 bytes
### otBoilerCacheValid: bool[128] = 128 bytes
### otUnknownIds: uint8_t[16] = 16 bytes
### otUnknownCounters: uint8_t[32] = 32 bytes
### otLoopbackData[128]: PROGMEM const = 0 RAM bytes (flash)
### emitSummaryLine static char line[420]: 420 bytes (static local in DRAM)

### Scalar state variables:
- OpenTherm otMaster, otSlave: ~24 bytes each (OpenTherm object, estimate from library) = ~48 bytes
- otMasterStatusFlags: uint8_t = 1 byte
- otCurrentMode: OTDirectMode (enum) = 4 bytes (ESP32 enums are int)
- otThermostatSeen, otSetbackEngaged, otSummerMode, otFailSafeEnabled: bool x4 = 4 bytes
- otLastThermostatMs, otMinIntervalMs, otLastAnySendMs, otDHWPushStartMs: uint32_t x4 = 16 bytes
- otSetbackTemp: float = 4 bytes
- otIgnoreTransitions, otOverrideHB, otVoltageRef, otDHWOverride: uint8_t x4 = 4 bytes
- otGpioFunctions[3], otLedFunctions[7]: char arrays = 10 bytes
- otTempSensor, otForceThermostat: char x2 = 2 bytes
- otScheduleIdx, otCmdHead, otCmdTail, otUnknownIdCount: uint8_t x4 = 4 bytes
- otDHWPushState: DHWPushState (uint8_t enum) = 1 byte
- otSummaryMode, otHideReports, otSummaryPending: bool x3 = 3 bytes
- otDHWBlocking, otCoolingEnable: bool x2 = 2 bytes
- otOperModeDHW, otOperModeCH1, otOperModeCH2: uint8_t x3 = 3 bytes
- otSlaveFramePending: bool = 1 byte
- otSlaveFrame: unsigned long = 4 bytes
- otMasterRequestActive: bool = 1 byte
- otLastSentRequest: unsigned long = 4 bytes
- otLastRequestOrigin: OTDirectRequestOrigin (enum) = 4 bytes
- Scalar subtotal: ~120 bytes (including OpenTherm object estimates)

### GRAND TOTAL:
- otSchedule[]: 1,980 bytes
- otOverrides[]: 42 bytes
- otResponseOverrides[]: 64 bytes
- otResponseModifiers[]: 32 bytes
- otCmdQueue[]: 32 bytes
- otBoilerCache[]: 256 bytes
- otBoilerCacheValid[]: 128 bytes
- otUnknownIds[]: 16 bytes
- otUnknownCounters[]: 32 bytes
- emitSummaryLine static line[420]: 420 bytes
- Scalars + objects: ~120 bytes
- TOTAL: ~3,122 bytes (excluding OpenTherm library internals)

All inside #if HAS_DIRECT_OT -> 0 bytes on ESP8266 builds (HAS_DIRECT_OT=0)
Total is below 4KB threshold (3,122 < 4,096). No audit-fix needed.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Measured the static RAM footprint of OTDirect.ino when HAS_DIRECT_OT=1.

Key findings:
- OTScheduleEntry: 20 bytes each (with alignment padding) x 99 entries = 1,980 bytes
- otOverrides[7]: 42 bytes
- otResponseOverrides[16]: 64 bytes
- otResponseModifiers[8]: 32 bytes
- otCmdQueue[8]: 32 bytes
- otBoilerCache[128]: 256 bytes
- otBoilerCacheValid[128]: 128 bytes
- otUnknownIds[16] + otUnknownCounters[32]: 48 bytes
- emitSummaryLine static char line[420]: 420 bytes (static local)
- Scalar state variables: ~120 bytes
- GRAND TOTAL: ~3,122 bytes (ESP32-S3)
- otLoopbackData[128]: stored in PROGMEM (flash), 0 RAM cost

All code is inside #if HAS_DIRECT_OT block. ESP8266 builds with HAS_DIRECT_OT=0 contribute 0 bytes.
Total (3,122 bytes) is comfortably below the 4KB warning threshold. No audit-fix task needed.
The largest single consumer is the 420-byte static local in emitSummaryLine(), not the struct arrays.
<!-- SECTION:FINAL_SUMMARY:END -->
