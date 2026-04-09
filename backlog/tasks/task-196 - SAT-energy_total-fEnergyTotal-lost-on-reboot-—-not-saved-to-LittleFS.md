---
id: TASK-196
title: 'SAT: energy_total (fEnergyTotal) lost on reboot — not saved to LittleFS'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-04-09 05:21'
updated_date: '2026-04-09 05:36'
labels:
  - audit-fix
dependencies: []
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
state.sat.fEnergyTotal accumulates kWh consumed and is published to sat/energy_total (retained MQTT). It is transient state and is never saved to LittleFS settings.ini. A reboot resets the counter to 0.0f. The PID state IS saved via satSavePidState() to /sat_pid_state.json, but energy is not. This is a data loss gap for the HA energy dashboard integration. Both platforms are equally affected.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Add energy_total to a separate /sat_energy.json state file (like /sat_pid_state.json)
- [ ] #2 Load energy on boot in satLoadPidState() or equivalent satLoadEnergyState()
- [ ] #3 Save energy periodically (e.g. every hour) and on SAT disable
- [ ] #4 Verified on both ESP8266 and ESP32 builds
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Add satSaveEnergyState() and satLoadEnergyState() functions following /sat_pid_state.json pattern
2. Use /sat_energy.json file with {"kwh":X.XXX} format
3. Load energy on boot in initSAT() after satLoadPidState()
4. Save every hour (3600000ms timer) and on satDisable()
5. Add _energyLastSaveMs static tracker alongside _pidLastSaveMs
6. Both functions are symmetric to satSavePidState/satLoadPidState
<!-- SECTION:PLAN:END -->
