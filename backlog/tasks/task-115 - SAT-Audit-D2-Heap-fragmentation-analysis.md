---
id: TASK-115
title: 'SAT Audit D2: Heap fragmentation analysis'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-08 21:53'
updated_date: '2026-04-09 05:22'
labels:
  - audit
  - sat
  - phase-4
  - memory
milestone: m-0
dependencies: []
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Analyze heap fragmentation risk in the SAT C++ implementation. Check dynamic allocations, String class usage and long-running allocation patterns.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 All dynamic allocations in SAT code identified
- [x] #2 String class usage in hot paths found and documented
- [x] #3 Heap fragmentation risk assessed
- [x] #4 Follow-up fix tasks created with label 'audit-fix' for problems found
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Search SAT*.ino for String class usage\n2. Search for new/malloc/free\n3. Classify as hot-path vs one-off\n4. Evaluate fragmentation risk
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
## Heap Fragmentation Analysis

### String class usage in SAT files

SATble.ino (ESP32-only, #if defined(ESP32)):
- Line 182: String svcDataStr = advertisedDevice.getServiceData() -- BLE callback, only on ESP32
- Line 192: String uuidArdu = svcUUID.toString() -- BLE callback, ESP32 only
- Line 211: String macArdu = advertisedDevice.getAddress().toString() -- BLE callback, ESP32 only
  NOTE: These are all inside BLE scan callback (ISR-adjacent, called from BLE stack). Each BLE
  advertisement triggers alloc/free of these String objects. On ESP32 heap this can fragment over time.
  On ESP8266 these paths are compiled out entirely.

SATweather.ino:
- Line 144: String payload = http.getString() -- One-off HTTP fetch, commented as ADR-004 exception.
  This is acceptable per project rules (setup/one-off context, not hot path).

SATcontrol.ino:
- Line 494: Comment only ("String labels for boiler status (PROGMEM)") -- no actual String usage.

### Dynamic allocation (new/malloc)
- None found in SAT files outside of ESP32 BLE library internals.

### Hot path check
- satControlLoop() is called every 30s (timer-guarded) -- no String or heap alloc in this path.
- satCycleSample() is called every loop iteration -- no String or heap alloc.
- satCycleOnFlameChange() -- no String or heap alloc.

### Risk assessment
- ESP8266: No heap fragmentation risk from SAT code. All String usage is in ESP32-only paths.
- ESP32: BLE callback allocates 3 String objects per discovered device advertisement. With scanning
  every 30s for 3s windows, and potentially many nearby BLE devices, this could cause fragmentation
  on ESP32 over time. Requires a fix task.
- weatherFetch() String is acceptable as a one-off per ADR-004 exception.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Heap fragmentation audit of SAT files complete.

Findings:
- ESP8266: No heap fragmentation risk. SAT code uses char[] buffers exclusively on this platform.
- ESP32: BLE scan callback in SATble.ino allocates 3 String objects per BLE advertisement, creating repeated alloc/free cycles during 3-second scan windows. This is a real fragmentation risk on ESP32 over extended uptime.
- weatherFetch() uses String for the HTTP response body -- this is an explicit ADR-004 exception (one-off, setup path) and is acceptable.
- No new/malloc/free usage found in SAT code outside of library internals.

Fix task created: TASK-202 (replace BLE String objects with char[] in ESP32 BLE callback).
<!-- SECTION:FINAL_SUMMARY:END -->
