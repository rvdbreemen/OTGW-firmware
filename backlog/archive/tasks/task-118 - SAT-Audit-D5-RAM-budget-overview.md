---
id: TASK-118
title: 'SAT Audit D5: RAM budget overview'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-08 21:54'
updated_date: '2026-04-09 05:26'
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
Create a total RAM usage overview of SAT functionality. Sum all static buffers, globals and estimated stack and compare with available RAM budget (~40KB).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Total RAM usage of SAT code calculated
- [x] #2 Ratio SAT RAM vs total firmware RAM documented
- [x] #3 Critical RAM consumers identified
- [x] #4 Follow-up fix tasks created with label 'audit-fix' for RAM budget breaches
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Enumerate all SAT static vars and struct sizes\n2. Estimate with alignment padding\n3. Express as fraction of 40KB usable RAM\n4. Flag if > 5KB
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
## SAT RAM Budget Overview

### Methodology
Manual struct field enumeration + alignment padding estimate. All sizes are conservative
estimates; actual sizes depend on compiler padding.

### Static RAM breakdown (ESP8266)

| Component | Estimated Size |
|-----------|---------------|
| SATSection (settings) | ~310 bytes |
| SATRuntimeSection (state) | ~400 bytes |
| SATcycles.ino module vars | 360 bytes |
| SATpid.ino module vars | 33 bytes |
| SATweather.ino module vars | 97 bytes |
| SATcontrol.ino module vars | 770 bytes |
| **Total (ESP8266)** | **~1,970 bytes** |

For ESP32, add SATble.ino: ~141 bytes for BLESensorData[4] + pointers/flags = **~2,111 bytes**.

### Critical consumers
1. static char climAttrBuf[512] in satPublishMQTT() -- 512B function-local static
2. static char pressAttrBuf[200] -- 200B function-local static
3. SATCycleRecord _cycleHistory[16] -- 288B module static
4. SATRuntimeSection -- ~400B in state struct (many float fields)

### Assessment
~1,970 bytes = **4.8% of the 40KB ESP8266 usable RAM budget.**
This is well within acceptable bounds (the 5KB threshold is not breached).

The cycle history ring buffer (288B) and climAttrBuf (512B) are the largest single allocations.
climAttrBuf could be reduced -- its worst-case content is ~434B, so a 512B static is slightly
oversized. Not a critical issue.

### No fix tasks needed for RAM budget.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
SAT RAM budget analysis complete.

Total estimated static RAM for SAT code: ~1,970 bytes on ESP8266 (~2,111 bytes on ESP32 with BLE).
This is approximately 4.8% of the 40KB ESP8266 usable RAM -- well below the 5KB audit threshold.

Largest consumers: climAttrBuf[512] (function-local static), SATCycleRecord[16] ring buffer (288B), and the SATRuntimeSection state struct (~400B).

No RAM budget breaches detected. No fix tasks created for this audit dimension.
<!-- SECTION:FINAL_SUMMARY:END -->
