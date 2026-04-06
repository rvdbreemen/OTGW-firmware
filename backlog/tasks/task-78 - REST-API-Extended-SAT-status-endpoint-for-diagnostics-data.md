---
id: TASK-78
title: 'REST API: Extended SAT status endpoint for diagnostics data'
status: To Do
assignee: []
created_date: '2026-04-06 19:16'
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
- [ ] #1 GET /api/v2/sat/status returns current data (backwards compatible)
- [ ] #2 GET /api/v2/sat/status?detail=full returns extended diagnostics
- [ ] #3 Extended response includes: sync_setpoint, sync_modulation, sync_ch (bool problem indicators)
- [ ] #4 Extended response includes: pressure_smoothed, pressure_drop_rate, pressure_alarm
- [ ] #5 Extended response includes: cycle_kind, cycle_duration, cycle_samples, cycle_fractions
- [ ] #6 Extended response includes: error_recent_mean, error_daily_mean, error_daily_median, error_daily_samples
- [ ] #7 Extended response includes: flame_health, device_health, cycle_health
- [ ] #8 Extended response includes: auto_tune_score, auto_tune_cycles, ovp_phase
- [ ] #9 Response built with chunked JSON to avoid large stack buffers
<!-- AC:END -->
