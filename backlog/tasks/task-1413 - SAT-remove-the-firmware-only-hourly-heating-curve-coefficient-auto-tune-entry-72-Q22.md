---
id: TASK-1413
title: >-
  SAT: remove the firmware-only hourly heating-curve coefficient auto-tune
  (entry 72, Q22)
status: To Do
assignee: []
created_date: '2026-10-28T06:44:00Z'
labels:
  - sat
  - sergeant-d
milestone: 2.0.0
dependencies:
  - TASK-1412.3
references:
  - docs/sat/sergeant-d-corrections.md
priority: medium
ordinal: 577000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeant D 2026-10-27 (entry 72, Q22: Remove). Remove the hourly coefficient auto-tune (SATcontrol.ino ~4798-4934, call at ~5252 inside if(roomValid)), its setting/state fields, REST/MQTT/HA discovery entities (retire the retained config, update docs/api/MQTT.md) and v2.js entries. Keep satautogains and the curve recommendation. Q21 (entry 72): 5.0 upper bound stays (ADR-198).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Host test: HEAD with SATautotune on rewrites fHeatingCurveCoeff after 1 h and 6 mostly-overshoot cycles; the fix never writes it (red then green)
- [ ] #2 A stored coefficient is unchanged across the upgrade
- [ ] #3 Retained HA discovery config for the removed entity is cleared; docs/api/MQTT.md updated
- [ ] #4 ADR-198 open question on auto-tune closed with entry 72
- [ ] #5 build.bat esp32-combo SUCCESS and evaluate.py --quick green
<!-- AC:END -->
