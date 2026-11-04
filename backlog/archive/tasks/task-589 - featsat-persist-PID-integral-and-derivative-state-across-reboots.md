---
id: TASK-589
title: 'feat(sat): persist PID integral and derivative state across reboots'
status: Done
assignee:
  - '@claude'
created_date: '2026-05-08 16:49'
updated_date: '2026-05-08 18:08'
labels: []
dependencies: []
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
The Python SAT persists integral accumulator, raw derivative, last temperature, and last timestamps to HA storage so the PID warm-up period is not lost on restart. The C++ port reinitializes all PID state to zero on reboot. This causes SAT control to be suboptimal for several minutes after every reboot (OTA, watchdog, power cycle) while the integral re-establishes. Field testers observe a cold-start undershoot pattern after each beta flash.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 PID integral (fPidI) saved to settings.sat or a separate persistence file on LittleFS
- [x] #2 Raw derivative and last-temperature saved alongside the integral
- [x] #3 Values restored on boot before first SAT control tick
- [x] #4 Graceful default (zero) when no persisted state exists
- [x] #5 Persisted state invalidated if SAT is disabled/reset via UI
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Add satPidRestoreState(float integral, float rawDerivative) to SATpid.ino — sets _pid_integral and _pid_rawDerivative directly\n2. Update satSavePidState() to include fRawDerivative (rd key) in the JSON\n3. Update satLoadPidState() to parse rd and call satPidRestoreState() instead of directly setting state.sat.fPidI\n4. Save immediately when integral is reset via satResetIntegral() so restore after UI reset gets zeros\n5. Build, bump alpha.32, commit, push
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Root cause: satLoadPidState() was setting state.sat.fPidI (a display field) instead of _pid_integral (the actual PID accumulator in SATpid.ino). Fix adds satPidRestoreState(integral, rawDerivative) in SATpid.ino that sets internal statics directly. Also saves rawDerivative (rd key) in the JSON. Also fixed satDisable() to save zeros AFTER satPidReset() instead of before, and added satSavePidState() call in satFlushShortLivedData() so manual flushes invalidate the file.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Fixed PID warm-start: satLoadPidState() was writing to state.sat.fPidI (display copy) instead of _pid_integral (the actual accumulator in SATpid.ino). Every reboot was a cold start despite the persistence file being written correctly.

Changes:
- SATpid.ino: added satPidRestoreState(float integral, float rawDerivative) that writes directly to _pid_integral and _pid_rawDerivative; _pid_initialized stays false so the first update cycle still seeds timestamps and lastRoomTemp from live data
- SATcontrol.ino: satSavePidState() buf 160→192, added rd (rawDerivative) field to JSON; satLoadPidState() parses rd and calls satPidRestoreState() instead of direct state field writes; satDisable() reordered to save zeros AFTER satPidReset(); satFlushShortLivedData() now calls satSavePidState() after reset so manual flushes invalidate the file

Build: python build.py exit 0, evaluate.py --quick no new failures. Shipped as alpha.32 (c650e28d) on origin/feature-dev-2.0.0-otgw32-esp32-sat-support.
<!-- SECTION:FINAL_SUMMARY:END -->
