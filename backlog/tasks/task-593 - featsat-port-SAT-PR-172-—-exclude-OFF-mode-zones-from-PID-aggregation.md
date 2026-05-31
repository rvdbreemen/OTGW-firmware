---
id: TASK-593
title: 'feat(sat): port SAT PR #172 — exclude OFF-mode zones from PID aggregation'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-05-08 18:00'
updated_date: '2026-05-31 22:15'
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
- [ ] #1 Zones in HVACMode.OFF (signal TBD) excluded from P75 aggregation in satControlLoop multi-zone block
- [ ] #2 satZonePidStep() returns SAT_MIN_SETPOINT for OFF-mode zones, same as invalid zones
- [ ] #3 PID integral reset when zone transitions from HEAT to OFF
- [ ] #4 Single-zone behavior unchanged
<!-- AC:END -->
