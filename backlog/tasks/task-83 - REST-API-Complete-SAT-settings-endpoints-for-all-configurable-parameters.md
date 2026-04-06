---
id: TASK-83
title: 'REST API: Complete SAT settings endpoints for all configurable parameters'
status: To Do
assignee: []
created_date: '2026-04-06 19:23'
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
- [ ] #1 Every MQTT command topic has a corresponding REST API endpoint
- [ ] #2 New endpoints under /api/v2/sat/settings/ namespace
- [ ] #3 Each endpoint validates input (range check, type check)
- [ ] #4 Each endpoint persists to flash and takes effect immediately
- [ ] #5 Returns JSON {status:ok, setting:name, value:new_value} on success
- [ ] #6 Returns JSON {status:error, message:reason} on failure
- [ ] #7 All endpoints documented in dispatch table for easy extension
- [ ] #8 WebUI can use these endpoints for all settings changes
<!-- AC:END -->
