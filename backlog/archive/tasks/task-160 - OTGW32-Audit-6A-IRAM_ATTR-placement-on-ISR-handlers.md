---
id: TASK-160
title: 'OTGW32-Audit-6A: IRAM_ATTR placement on ISR handlers'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-08 22:21'
updated_date: '2026-04-08 22:30'
labels:
  - audit
  - otgw32
  - phase-6
milestone: m-1
dependencies: []
references:
  - src/OTGW-firmware/OTDirect.ino
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Verify that masterISR() and slaveISR() are correctly annotated with IRAM_ATTR so they reside in IRAM on the ESP32-S3. On ESP32, ISRs called from GPIO interrupts must be in IRAM to avoid cache misses during flash reads. Also verify that any functions called from within the ISR context are also IRAM-safe.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 masterISR() has IRAM_ATTR annotation
- [x] #2 slaveISR() has IRAM_ATTR annotation
- [x] #3 OpenTherm::handleInterrupt() is IRAM-safe (library-level check)
- [x] #4 No IRAM_ATTR function calls non-IRAM functions directly
- [x] #5 Any missing IRAM_ATTR results in an audit-fix task
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
## Audit Findings

### masterISR() and slaveISR() — IRAM_ATTR (lines 31-32)
- Both ISR wrappers have IRAM_ATTR: `static void IRAM_ATTR masterISR()` and `static void IRAM_ATTR slaveISR()`.
- PASS.

### OpenTherm::handleInterrupt() — library check
- `OpenTherm.cpp` line 324: `void IRAM_ATTR OpenTherm::handleInterrupt()` — explicitly marked.
- `OpenTherm.h` line 278: `onTxTimer` is also `IRAM_ATTR` (ESP32 hardware-timer callback).
- `OpenTherm.h` lines 297-302: fallback `#define IRAM_ATTR ICACHE_RAM_ATTR` for non-ESP32 platforms — safe.
- `handleInterruptHelper` (line 370 in .cpp): `void IRAM_ATTR OpenTherm::handleInterruptHelper(void* ptr)` — IRAM.
- `isReady()` and `readState()` are also `IRAM_ATTR` (called from ISR path).
- PASS.

### Transitive call safety (AC4)
- `handleInterrupt()` body operates only on object members (bit-banging, state machine), no flash reads.
- No `DebugTln`, `String`, or other non-IRAM calls detected inside the ISR chain.
- PASS — no non-IRAM calls from IRAM context.

### Verdict
- All IRAM_ATTR annotations are correct. No audit-fix needed.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Audit of IRAM_ATTR placement on ISR handlers — all PASS, no issues found.

Findings:
- masterISR() (line 31) and slaveISR() (line 32) both carry IRAM_ATTR — correct.
- OpenTherm::handleInterrupt() in the library (OpenTherm.cpp line 324) is `void IRAM_ATTR` — correct.
- handleInterruptHelper(), isReady(), readState(), and onTxTimer() are all IRAM_ATTR in the library.
- Fallback #define for non-ESP32 platforms is safe (defines IRAM_ATTR as ICACHE_RAM_ATTR).
- No non-IRAM functions are called from within the ISR chain.

No audit-fix task required.
<!-- SECTION:FINAL_SUMMARY:END -->
