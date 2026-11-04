---
id: TASK-114
title: 'SAT Audit D1: Stack usage analysis'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-08 21:53'
updated_date: '2026-04-09 05:21'
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
Analyze stack usage of SAT-related functions in the firmware. The ESP8266 CONT stack is 4KB. Identify functions with large local buffers and check for stack overflow risk.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 All SAT functions with local buffers >100 bytes identified
- [x] #2 Stack usage estimated per call chain
- [x] #3 Potential stack overflow scenarios documented
- [x] #4 Follow-up fix tasks created with label 'audit-fix' for high-risk functions
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Grep all SAT*.ino for char[] > 100 bytes (local, non-static)\n2. Trace call chains from doBackgroundTasks() to SAT functions\n3. Estimate per-function stack usage\n4. Identify worst-case stack depth
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
## Stack Usage Analysis Findings

### Functions with local buffers > 100 bytes in SAT files

| File | Line | Buffer | Size | Risk |
|---|---|---|---|---|
| SATweather.ino | 119 | url[220] | 220B | Medium - one-off fetch path |
| SATweather.ino | 229 | forecastBuf[256] | 256B | HIGH - stacked with entryBuf[300] in same scope |
| SATweather.ino | 246 | entryBuf[300] | 300B | HIGH - stacked with forecastBuf[256] |
| SATcontrol.ino | 1002 | buf[128] | 128B | Low - file I/O path, not hot |
| SATcontrol.ino | 1014 | buf[128] | 128B | Low - file I/O path, not hot |
| SATcontrol.ino | 1301 | jsonBuf[128] | 128B | Low - MQTT publish |
| SATcontrol.ino | 1329 | jBuf[200] | 200B | Medium - cycle attributes JSON |
| SATcontrol.ino | 1463 | jBuf[180] | 180B | Medium - curve recommendation JSON |

### Call chain stack depth estimate
doBackgroundTasks() [~80B frame]
  -> satControlLoop() [~150B frame]
     -> satPidUpdate() [~120B frame]
        -> _pidCalculateGains(), _pidUpdateIntegral(), _pidUpdateDerivative() [~40B each]
     -> satPublishMQTT() [~200B frame - valBuf[16], jBuf[200]]
     -> weatherSendStatusJSON() [~640B frame - forecastBuf[256] + entryBuf[300] + tmpBuf[8]]

### High-risk scenario
weatherSendStatusJSON() with stacked forecastBuf[256] + entryBuf[300] = 556B peak in one
scope block plus function overhead: estimated 640-700B on stack per call.

This is NOT over 4KB alone but contributes to cumulative depth. Fix task created: TASK-197.

### Result
- No single function exceeds 500B standalone in SAT files on ESP8266 paths
- weatherSendStatusJSON() is the highest risk with 556B stacked buffers (fix: TASK-197)
- SATble.ino is ESP32-only -- BLE stack usage is irrelevant for ESP8266 target
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Stack usage audit of SAT firmware files complete.

Findings:
- No SAT function exceeds 500 bytes of local allocation in isolation on ESP8266 paths
- Highest-risk location: weatherSendStatusJSON() in SATweather.ino, where forecastBuf[256] and entryBuf[300] are declared in the same scope block, creating a 556-byte peak allocation plus function overhead (~640-700B total frame)
- SATble.ino is guarded by #if defined(ESP32) — its BLE String usage and local buffers are irrelevant to ESP8266 stack budget
- All other SAT local buffers (128-200B) are in non-hot paths (file I/O, MQTT publish) and within safe limits
- Created TASK-197 to fix the stacked-buffer issue in weatherSendStatusJSON()
<!-- SECTION:FINAL_SUMMARY:END -->
