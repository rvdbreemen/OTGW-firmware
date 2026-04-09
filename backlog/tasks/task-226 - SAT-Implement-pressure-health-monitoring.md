---
id: TASK-226
title: 'SAT: Implement pressure health monitoring'
status: To Do
assignee: []
created_date: '2026-04-09 05:30'
labels:
  - audit-fix
  - sat
  - pressure
dependencies: []
references:
  - backlog/docs/sat-feature-completeness-matrix.md
  - backlog/docs/sat-python-cpp-mapping.md
  - src/OTGW-firmware/SATcontrol.ino
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Python's SatPressureHealthSensor monitors boiler water pressure (OT MsgID 18) for health issues: pressure outside normal band, or a slow declining trend (leak/air). The C++ firmware does not monitor pressure at all.

Pressure health is directly safety-relevant: an undetected pressure drop can cause boiler lock-out or water damage. This is a meaningful safety gap.

Features to port:
- EMA-smoothed pressure reading from OT MsgID 18
- Min/max pressure band check (configurable, defaults ~0.8 and 2.5 bar)
- Linear regression over a sliding window to detect slow pressure drop rate
- 120s confirmation delay before flagging a problem (avoid transient false positives)
- MQTT publish of pressure health status and current pressure value

Complexity: Medium. OT MsgID 18 is already parsed by the firmware (check processOT). The EMA + linear regression on a sliding window is a modest addition to SATcontrol.ino.

Risk: Low. This is a new sensor feature; it does not affect the existing control loop. The pressure value is read-only from the boiler.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 OT MsgID 18 (water pressure) is read and stored in state.sat.fBoilerPressure
- [ ] #2 EMA-smoothed pressure is computed with alpha configurable via settings
- [ ] #3 Pressure is flagged LOW if below settings.sat.fPressureMin (default 0.8 bar)
- [ ] #4 Pressure is flagged HIGH if above settings.sat.fPressureMax (default 2.5 bar)
- [ ] #5 Linear regression over last 60 samples detects drop rate > threshold (configurable, default 0.01 bar/min)
- [ ] #6 Problem state must persist for 120 seconds before MQTT alert is published
- [ ] #7 Pressure health status is published to MQTT as a separate topic
- [ ] #8 No impact on the SAT control loop when pressure health is normal
<!-- AC:END -->
