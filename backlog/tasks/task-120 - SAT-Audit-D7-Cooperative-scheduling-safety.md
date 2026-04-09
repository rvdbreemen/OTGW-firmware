---
id: TASK-120
title: 'SAT Audit D7: Cooperative scheduling safety'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-08 21:54'
updated_date: '2026-04-09 05:28'
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
Verify SAT code is safe within the ESP8266 cooperative scheduling environment. Check for shared buffers across yield boundaries, re-entrancy of doBackgroundTasks() and timer usage.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 All shared state in SAT code identified
- [x] #2 Potential re-entrancy problems documented
- [x] #3 Timer usage follows DECLARE_TIMER_SEC/DUE() pattern
- [x] #4 Follow-up fix tasks created with label 'audit-fix' for safety issues
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Check all SAT code paths for blocking operations > 50ms\n2. Verify timer usage pattern (DECLARE_TIMER_SEC/DUE)\n3. Check re-entrancy safety of shared state\n4. Verify yield/feedWatchDog placement
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
## Cooperative Scheduling Safety Audit

### Blocking operations check

1. **weatherFetch() in SATweather.ino**
   - http.setTimeout(10000) -- 10-second HTTP timeout
   - This CAN block for up to 10 seconds if the Open-Meteo API is slow or unreachable.
   - Guards: yield() before and after http.GET(); feedWatchDog() after http.end().
   - weatherFetch() is called from weatherLoop() which is guarded by DUE(timerWeatherPoll) -- only runs every 15min minimum.
   - ISSUE: A 10-second block is well over the 50ms limit for cooperative scheduling. The ESP8266 watchdog timer fires at 8 seconds by default, and feedWatchDog() is called AFTER the GET(), not during it. If the server takes 9-10s, the WDT may fire.
   - This is a known risk for any blocking HTTP call on ESP8266. On ESP32 this is less critical (has RTOS).

2. **satControlLoop()** -- executes lightweight math, no blocking calls. Estimated <1ms per call.

3. **satCycleSample()** -- called every doBackgroundTasks() iteration; pure arithmetic, no blocking. Estimated <0.1ms.

4. **satBLELoop()** -- ESP32 only. Uses _pBLEScan->start(3, false) with false = non-blocking (async callback). Correct.

5. **satSavePidState() / satLoadPidState()** -- LittleFS file I/O. Typically <5ms. No yield(), but only called on disable/enable transitions, not hot path.

### Timer pattern verification
- satControlLoop: uses DECLARE_TIMER_SEC(timerSATControl, ...) + DUE() -- CORRECT
- weatherLoop: uses DECLARE_TIMER_SEC(timerWeatherPoll, ...) + DUE() -- CORRECT
- No SAT code uses raw millis() comparisons for rate-limiting (all use DUE() pattern)

### Re-entrancy safety
- From MEMORY.md: doBackgroundTasks() can be re-entered via doAutoConfigure() -> handleOTGW() -> processOT()
- SAT module statics (_cycle_*, _pid_*, _sustain_*, _ema_*) could be touched from the re-entrant path IF processOT() calls SAT functions.
- Verified: processOT() calls satCycleOnFlameChange() and satCycleSample() indirectly via event callbacks?
- CHECK: Does OTGW-Core.ino call any SAT functions directly?

### yield() placement in weatherFetch
- yield() before http.GET() is useless (GET hasnt started yet)
- feedWatchDog() is called after http.end() -- correct for recovery but too late for WDT protection during GET

### Re-entrancy conclusion
- satDetectManufacturer() is the ONLY SAT function called from processOT()/OTGW-Core.
- satDetectManufacturer() only writes to state.sat.iDetectedManufacturer and state.sat.iSlaveMemberID -- simple assignments, no buffers shared with satControlLoop().
- satCycleOnFlameChange() and satCycleSample() are NOT called from OTGW-Core, only from satControlLoop() and doBackgroundTasks().
- Therefore: NO re-entrancy risk in SAT module statics from the doAutoConfigure() re-entrancy path.

### Final findings
1. weatherFetch() 10-second HTTP timeout can block ESP8266 cooperative scheduler -- TASK-221 created.
2. Timer pattern (DECLARE_TIMER_SEC/DUE) used correctly throughout SAT code.
3. BLE scan is correctly non-blocking (false flag to start()).
4. No re-entrancy risks found in SAT module state.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Cooperative scheduling safety audit complete.

Findings:
1. weatherFetch() blocks for up to 10 seconds (HTTP timeout) without WDT protection on ESP8266. The feedWatchDog() is called after the GET completes, not during it. WDT fires at ~8s, making a 10s timeout potentially fatal. Created TASK-221.

2. Timer pattern is correct: both satControlLoop and weatherLoop use DECLARE_TIMER_SEC/DUE() -- no polling busy-loops found.

3. BLE scan (ESP32): non-blocking mode confirmed (_pBLEScan->start(3, false)).

4. Re-entrancy: only satDetectManufacturer() is called from processOT(). It writes only 2 simple state fields with no shared buffer risk. No re-entrancy hazards in SAT module statics.

5. File I/O (satSavePidState/satLoadPidState): blocking but only on enable/disable transitions, not hot path.
<!-- SECTION:FINAL_SUMMARY:END -->
