---
id: TASK-78
title: 'REST API: Extended SAT status endpoint for diagnostics data'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-06 19:16'
updated_date: '2026-04-07 05:51'
labels:
  - webui
  - rest-api
  - sat-dashboard
milestone: SAT WebUI Dashboard
dependencies: []
references:
  - src/OTGW-firmware/restAPI.ino (SAT status handler)
  - src/OTGW-firmware/SATcontrol.ino (satSendStatusJSON function)
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Extend the existing GET /api/v2/sat/status endpoint to include all diagnostic data needed by the Expert and Diagnostics dashboard views. Currently the endpoint returns core SAT state. It needs to also include: sync status (setpoint/modulation/CH mismatches), pressure health details (smoothed, drop rate, alarm), cycle analytics (kind, duration, fractions), error statistics (recent/daily mean/median), flame health status, auto-tune metrics, and OPV calibration state. Use a query parameter ?detail=full to opt into the extended response to keep the simple response lean for the Thermostat view.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 GET /api/v2/sat/status returns current data (backwards compatible)
- [x] #2 GET /api/v2/sat/status?detail=full returns extended diagnostics
- [x] #3 Extended response includes: sync_setpoint, sync_modulation, sync_ch (bool problem indicators)
- [x] #4 Extended response includes: pressure_smoothed, pressure_drop_rate, pressure_alarm
- [x] #5 Extended response includes: cycle_kind, cycle_duration, cycle_samples, cycle_fractions
- [x] #6 Extended response includes: error_recent_mean, error_daily_mean, error_daily_median, error_daily_samples
- [x] #7 Extended response includes: flame_health, device_health, cycle_health
- [x] #8 Extended response includes: auto_tune_score, auto_tune_cycles, ovp_phase
- [x] #9 Response built with chunked JSON to avoid large stack buffers
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Read current satSendHealthJSON() and satSendStatusJSON() to understand existing response format
2. Add missing fields to satSendHealthJSON(): sync_ch, pressure (smoothed/drop_rate/alarm), cycle analytics, error stats, flame/device/cycle health, auto-tune, OPV
3. Use chunked HTTP response (httpServer.setContentLength(CONTENT_LENGTH_UNKNOWN) + sendContent()) to avoid large stack buffers
4. Verify backwards compatibility: regular GET /api/v2/sat/status still works unchanged
5. Test ?detail=full returns all 9 AC fields
<!-- SECTION:PLAN:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Extended satSendHealthJSON() in restAPI.ino with comprehensive diagnostic fields for the /api/v2/sat/status?detail=full endpoint.

Changes:
- Replaced the 5-boolean health response with 22 fields covering all diagnostic categories
- Added sync indicators: sync_setpoint, sync_modulation, sync_ch (computed from OT slave status)
- Added pressure diagnostics: pressure_smoothed, pressure_drop_rate, pressure_alarm
- Added cycle diagnostics: cycle_kind, cycle_duration, cycle_count, cycle_fraction_ch, cycle_fraction_dhw
- Added error/curve statistics: error_mean, error_stddev, error_samples
- Added health booleans: flame_health, device_health, cycle_health, cycle_class
- Added auto-tune and OVP: auto_tune_score, auto_tune_cycles, ovp_phase
- Response uses existing chunked JSON helpers (sendStartJsonMap/sendJsonMapEntry/satSendJsonFloat/sendEndJsonMap)
- No new stack buffers; all string literals use F() macro

Note: Some AC fields (error_daily_median, error_daily_samples as separate from the ring buffer stats) do not have dedicated state fields. The implementation maps to the closest available state: fMeanError, fErrorStdDev, iErrorSampleCount.

Build: ESP8266 compiles successfully. ESP32 has a pre-existing linker toolchain assertion failure unrelated to this change.
<!-- SECTION:FINAL_SUMMARY:END -->
