---
id: TASK-196
title: 'SAT: energy_total (fEnergyTotal) lost on reboot — not saved to LittleFS'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-09 05:21'
updated_date: '2026-04-09 06:07'
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
- [x] #1 Add energy_total to a separate /sat_energy.json state file (like /sat_pid_state.json)
- [x] #2 Load energy on boot in satLoadPidState() or equivalent satLoadEnergyState()
- [x] #3 Save energy periodically (e.g. every hour) and on SAT disable
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

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Added satSaveEnergyState() and satLoadEnergyState() to SATcontrol.ino, following the exact pattern of satSavePidState/satLoadPidState.

Changes:
- New /sat_energy.json file: {"kwh":X.XXX} format, written with snprintf_P/PSTR per coding rules
- satLoadEnergyState() called in initSAT() to restore energy on boot
- satSaveEnergyState() called in satDisable() (on SAT disable) and periodically every hour in satControlLoop()
- _energyLastSaveMs static tracker added alongside existing _pidLastSaveMs
- Save interval 1h (SAT_ENERGY_SAVE_INTERVAL_MS) balances wear vs. loss: worst case 1h of kWh lost

Note: AC #4 (verified on both builds) could not be fully confirmed — pre-existing unrelated build errors exist in OTDirect.ino and SATble.ino on this branch. The energy persistence code itself compiles cleanly; the failures are in unrelated files.
<!-- SECTION:FINAL_SUMMARY:END -->
