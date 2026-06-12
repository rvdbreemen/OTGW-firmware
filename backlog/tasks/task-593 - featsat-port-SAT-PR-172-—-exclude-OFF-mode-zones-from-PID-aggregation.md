---
id: TASK-593
title: 'feat(sat): port SAT PR #172 — exclude OFF-mode zones from PID aggregation'
status: Done
assignee:
  - '@claude'
created_date: '2026-05-08 18:00'
updated_date: '2026-06-01 06:12'
labels: []
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
SAT Python HA integration PR #172 (github.com/Alexwijn/SAT/pull/172) fixes a bug where a zone whose thermostat is set to HVACMode.OFF still contributes its stale error to the PID max_error calculation. In area.py, the fix is to return None for state/error/weight when HVACMode.OFF.\n\nIn the C++ firmware, the equivalent is satZonePidStep() and the P75 aggregation block. When a zone thermostat is OFF, the zone should be excluded from the P75 zone outputs entirely.\n\nBLOCKED: Need to confirm from sergeantd how HVACMode.OFF is signalled to the firmware — via zone setpoint going absent (bSpValid=false after timeout), explicit MQTT zero setpoint, or a dedicated zone mode topic.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Zones in HVACMode.OFF (signal TBD) excluded from P75 aggregation in satControlLoop multi-zone block
- [x] #2 satZonePidStep() returns SAT_MIN_SETPOINT for OFF-mode zones, same as invalid zones
- [x] #3 PID integral reset when zone transitions from HEAT to OFF
- [x] #4 Single-zone behavior unchanged
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Implemented + committed a3aa6672 (alpha.118), pushed. bOff zone flag + satHandleZoneMode() + satZonePidExclude() helper; OFF zone returns SAT_MIN_SETPOINT -> drops from P75 via existing zOut gate; PID integral reset on HEAT->OFF; diagnostics active count excludes OFF; single-zone path untouched. Build esp8266+esp32 SUCCESS (fw+fs), evaluate --quick 0 failed. OPEN QUESTION for maintainer: OFF signal is driven through satHandleZoneMode(value) accepting HA HVACMode strings ('off' vs heat/auto/...); confirm the actual MQTT/zone-mode topic + payload wiring matches the SAT HA integration. AC marked code-complete; field-validation by sergeantd still advisable.
<!-- SECTION:NOTES:END -->
