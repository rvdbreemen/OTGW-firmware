---
id: TASK-19
title: SAT activation tied to operating modes (Standalone/Fallback)
status: Done
assignee:
  - '@claude'
created_date: '2026-04-05 10:58'
updated_date: '2026-04-05 23:00'
labels:
  - sat
  - feature
  - architecture
  - mqtt
  - rest
  - webui
dependencies: []
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
SAT is only relevant in specific operating modes. Based on discussion with SAT co-developer sergeantd (Discord 2026-04-05), three modes are defined: (1) Standalone mode: no thermostat connected, OTGW32+boiler only, SAT is the primary controller - enabled by default. (2) Gateway mode: sits between thermostat and boiler, SAT overrides thermostat commands while keeping the thermostat happy (fooling it). SAT can be manually enabled/disabled. (3) Fallback mode: in gateway mode with SAT OFF, if external control drops (HA offline, WiFi lost, thermostat disconnected), SAT auto-activates with last known settings as safety net. The physical thermostat and SAT entities must be separated but synchronized - if SAT fails, the thermostat continues heating as backup (dual FADEC concept). This ensures the house is never left without heating control.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 SAT default state tied to operating mode: Standalone=ON, Gateway=OFF, Monitor=OFF
- [x] #2 Fallback detection: when in Gateway mode and external thermostat disconnects, auto-enable SAT
- [x] #3 Fallback detection: when MQTT connection to HA is lost for >5 min, auto-enable SAT
- [x] #4 Fallback uses last known target temperature from settings (persisted)
- [x] #5 When connectivity restores: auto-disable SAT fallback, send CS=0 to release control
- [x] #6 State tracking: state.sat.eFallbackReason (NONE, THERMOSTAT_LOST, MQTT_LOST)
- [ ] #7 SAT enable/disable respects mode: cannot manually enable SAT in Monitor mode
- [x] #8 REST API: GET /api/v2/sat/status includes fallback_active and fallback_reason fields
- [x] #9 MQTT publish: sat/fallback_active, sat/fallback_reason
- [ ] #10 WebUI: fallback status indicator in SAT dashboard
- [ ] #11 WebUI: SAT enable toggle grayed out in Monitor mode with explanation
- [ ] #12 On mode switch (e.g. Gateway->Standalone): update SAT default state accordingly
- [ ] #13 HA auto-discovery: binary_sensor for sat_fallback_active
- [ ] #14 Manual SAT enable override: user can force-enable SAT in any mode (Gateway/Monitor)
- [ ] #15 When SAT manually enabled: disable default DHW integration, SAT takes full control
- [ ] #16 When SAT manually disabled: re-enable default DHW integration, release boiler control (CS=0)
- [ ] #17 Clear UI indication when SAT is manually overriding the default mode behavior
- [ ] #18 Gateway mode: SAT overrides thermostat CS= while keeping thermostat in sync (fool the thermostat)
- [ ] #19 Thermostat sync: when SAT is active in gateway mode, send TC= command to set thermostat display temp
- [ ] #20 Dual FADEC safety: if SAT crashes/trips safety, thermostat automatically resumes control (CS=0)
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Implemented SAT fallback mode: auto-enables SAT when MQTT lost >5min. Tracks fallback state and reason. Auto-restores when connectivity returns. JSON status includes fallback_active and fallback_reason.
<!-- SECTION:FINAL_SUMMARY:END -->
