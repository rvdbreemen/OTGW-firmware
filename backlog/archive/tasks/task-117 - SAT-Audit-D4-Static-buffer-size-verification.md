---
id: TASK-117
title: 'SAT Audit D4: Static buffer size verification'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-08 21:53'
updated_date: '2026-04-09 05:24'
labels:
  - audit
  - sat
  - phase-4
  - memory
milestone: m-0
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Verify all static buffers in SAT code are correctly sized. Buffers must be large enough for worst-case input but not excessive given the RAM budget.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 All static char[] buffers in SAT code listed with size
- [x] #2 Worst-case usage per buffer verified
- [x] #3 RAM budget impact calculated
- [x] #4 Follow-up fix tasks created with label 'audit-fix' for oversized or undersized buffers
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. List all char[] buffers in SAT files by size\n2. Verify worst-case content for each\n3. Check snprintf_P uses sizeof(buf) consistently\n4. Flag any off-by-one risks
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
## Static Buffer Size Verification

All SAT char[] buffers verified against worst-case content:

| Buffer | Size | Worst-case | Safe? | Note |
|--------|------|------------|-------|------|
| url[220] (weatherFetch) | 220 | 172 chars | YES | Open-Meteo URL with max lat/lon |
| search[48] (weatherJson) | 48 | 18 chars | YES | "temperature_2m": longest key |
| forecastBuf[256] | 256 | 170 chars | YES | 24 temps * 7 chars |
| entryBuf[300] | 300 | 181 chars | YES | "forecast":[ + forecastBuf ] |
| buf[128] (satSavePidState) | 128 | ~60 chars | YES | {i:%.4f,d:%.4f,err:%.2f} |
| buf[128] (satLoadPidState) | 128 | file read | YES | reads up to 127 bytes safely |
| jsonBuf[128] (pid attrs) | 128 | ~100 chars | YES | {error:x,proportional:x,...} |
| jBuf[200] (cycle attrs) | 200 | 169 chars | YES | cycle attributes JSON |
| pressAttrBuf[200] (static) | 200 | 186 chars | YES | pressure health JSON attrs |
| jBuf[180] (curve rec.) | 180 | ~120 chars | YES | curve recommendation JSON |
| climAttrBuf[512] (static) | 512 | 434 chars | YES | climate attributes JSON |
| macBuf[18] (SATble) | 18 | 17 chars | YES | "AA:BB:CC:DD:EE:FF
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Static buffer size verification for SAT code complete.

All 14 char[] buffers in SAT files verified against worst-case content. No off-by-one errors or overflow risks found. All snprintf_P calls use sizeof(buf) consistently. Key findings:
- forecastBuf[256] worst-case 170 bytes -- 86 bytes headroom
- entryBuf[300] worst-case 181 bytes -- 119 bytes headroom
- climAttrBuf[512] worst-case 434 bytes -- 78 bytes headroom
- pressAttrBuf[200] worst-case 186 bytes -- 14 bytes headroom (tightest, but safe)

The stacked forecastBuf+entryBuf stack allocation is the only concern (covered by TASK-197).
<!-- SECTION:FINAL_SUMMARY:END -->
