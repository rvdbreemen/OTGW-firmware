---
id: TASK-203
title: 'SAT fix: missing per-hour PWM cycle count limiter'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-09 05:22'
updated_date: '2026-04-09 05:47'
labels:
  - audit-fix
dependencies: []
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Python PWM implementation tracks _current_cycle per rolling 60-minute window and stops new PWM cycles when max cycles_per_hour is reached (line ~156-160 pwm.py). C++ satApplyPWM() only uses duty-cycle math to limit frequency but has no hard cycle counter. If the duty cycle math produces very short on/off cycles, C++ will exceed the configured cycles_per_hour limit.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 C++ PWM mode tracks flame-on events per rolling 60-minute window
- [x] #2 When cycle count reaches satGetMaxCyclesPerHour(), new PWM ON cycles are suppressed
- [x] #3 Rolling window resets every 3600s matching Python behavior
- [x] #4 Cycle count is visible in SAT status JSON and MQTT
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Add SAT_MAX_CYCLES_PER_HOUR ring buffer (6 timestamps) in SATcycles.ino
2. _hourCountRecord(): push flame-on timestamp on cycle start
3. _hourCountGet(): count timestamps within 3600s window
4. satCycleIsHourLimitReached(): public gate checked in satApplyPWM OFF->ON transition
5. satCycleGetCyclesThisHour(): public accessor for JSON/MQTT
6. Add state.sat.iCyclesThisHour field to OTGWFirmware.h
7. Hook suppression into satApplyPWM (SATcontrol.ino) at the OFF-phase restart point
8. Expose in status JSON and MQTT
<!-- SECTION:PLAN:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Added per-hour PWM cycle counter to SATcycles.ino matching Python pwm.py rolling-window behavior.

Changes:
- SATcycles.ino: SAT_MAX_CYCLES_PER_HOUR (6-slot) ring buffer of flame-on timestamps (_hourCycleTs[])
- _hourCountRecord(): called on each flame-ON event in satCycleOnFlameChange()
- _hourCountGet(): counts timestamps still within 3600000ms (rolling window, no reset)
- satCycleIsHourLimitReached(): public gate — returns true when count >= satGetMaxCyclesPerHour()
- satCycleGetCyclesThisHour(): public accessor for status reporting
- OTGW-firmware.h: added state.sat.iCyclesThisHour (uint8_t) to SATSection
- SATcontrol.ino: satApplyPWM() now checks satCycleIsHourLimitReached() before starting a new ON cycle in the OFF-phase restart branch; logs once per suppression event via a static latch
- Status JSON: cycles_this_hour field added
- MQTT: sat/cycles_this_hour topic added

User impact: PWM mode now enforces the configured cycles_per_hour hard limit. Without this, the duty-cycle math alone could produce more cycles than configured if on/off times are very short.
<!-- SECTION:FINAL_SUMMARY:END -->
