---
id: TASK-1353
title: 'SAT: published heating curve shows the last zone''s curve when zones are on'
status: Done
assignee:
  - '@claude'
created_date: '2026-10-24T16:28:00Z'
updated_date: '2026-10-24T18:17:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 488000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
SAT port audit 2026-10-24 (docs/sat/sat-port-audit-2026-10-24.md) finding #66: SATcontrol.ino:598, 1996 publish the last zone's curve, misleading curve tuning. Display only.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Published curve is the main (non-zone) curve with zones on (host test red/green)
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
With zones on, the published heating curve (MQTT sat/heating_curve, REST, WS, debug) is the main curve again, not the last zone's. satCalcHeatingCurve() is now pure; the main call site stores the value. Control unchanged (PIDs use local curves). thermo-nova publishes the climate's own curve (climate.py:249/:697, sensor.py:291; areas keep their own, area.py:49/:262). Evidence: tests/test_sat_published_curve.py (real code cut) 5/5 on fix, HEAD 2 failures (published 29.1/35.5 vs main 32.3); second reader found no defect; build SUCCESS; evaluate 0 failed.
<!-- SECTION:FINAL_SUMMARY:END -->
