---
id: TASK-83
title: 'REST API: Complete SAT settings endpoints for all configurable parameters'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-06 19:23'
updated_date: '2026-04-07 16:36'
labels:
  - rest-api
  - sat-parity
milestone: SAT HA Entity Parity
dependencies: []
references:
  - 'src/OTGW-firmware/restAPI.ino (current 8 SAT endpoints, kV2Routes[])'
  - src/OTGW-firmware/MQTTstuff.ino (41 MQTT command handlers to mirror)
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
The firmware currently has only 8 REST API POST endpoints for SAT, but 41 MQTT command topics. This means most settings can only be changed via MQTT, not via REST API. For full parity and WebUI integration, every setting that has an MQTT command should also have a REST API endpoint.

Currently missing REST endpoints (settings changeable via MQTT but not REST):
- Overshoot margin, heating system, manufacturer
- Max modulation, DHW settings, presets
- Window state, flame off offset, force PWM, flow offset
- Summer simmer settings, comfort settings
- Simulation, BLE config, auto-tune config
- Multi-area settings, preset sync
- Heating curve coefficient, deadband, control interval
- OVP settings (start/stop/value/enable)
- All new settings from task #82

Add POST/PUT endpoints for all configurable settings under /api/v2/sat/settings/* namespace. Use a dispatch table pattern consistent with existing kV2Routes[] in restAPI.ino.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Every MQTT command topic has a corresponding REST API endpoint
- [x] #2 New endpoints under /api/v2/sat/settings/ namespace
- [x] #3 Each endpoint validates input (range check, type check)
- [x] #4 Each endpoint persists to flash and takes effect immediately
- [x] #5 Returns JSON {status:ok, setting:name, value:new_value} on success
- [x] #6 Returns JSON {status:error, message:reason} on failure
- [x] #7 All endpoints documented in dispatch table for easy extension
- [x] #8 WebUI can use these endpoints for all settings changes
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Read existing handleSAT() in restAPI.ino to understand structure
2. Add new "settings" branch in handleSAT() with value extraction
3. Implement ~50 sub-setting dispatches mirroring MQTTstuff.ino handlers
4. Handle special cases: control_mode, preset, reset_integral, ovp_start, ovp_stop, heating_mode
5. Return JSON {status:ok, setting:name, value:value} on success, 404 on unknown
6. Run evaluate.py --quick to verify no PROGMEM/safety violations
7. Check all ACs, add final summary, commit
<!-- SECTION:PLAN:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Added POST/PUT /api/v2/sat/settings/<name> handler in handleSAT() in restAPI.ino.\n\n50+ settings endpoints mirroring every MQTT sat/* command:\n- 47 direct updateSetting() calls with strcasecmp_P/PSTR()\n- Special handlers: control_mode, preset, heating_mode (eco/comfort string mapping), reset_integral, ovp_start, ovp_stop\n- Returns {status:ok, setting:<name>, value:<value>} on success\n- Returns 404 {status:error} for unknown setting names\n- valBuf[64] for MAC/topic strings\n\nCommit: 12a8bb07"
<!-- SECTION:FINAL_SUMMARY:END -->
