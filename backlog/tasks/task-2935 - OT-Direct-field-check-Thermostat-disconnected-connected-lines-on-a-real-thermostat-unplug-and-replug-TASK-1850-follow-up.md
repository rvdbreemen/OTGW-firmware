---
id: TASK-2935
title: >-
  OT-Direct field check: Thermostat disconnected/connected lines on a real
  thermostat unplug and replug (TASK-1850 follow-up)
status: To Do
assignee: []
created_date: '2026-11-19T22:38:00Z'
labels:
  - otdirect
  - parity
  - field
milestone: 2.1.0
dependencies: []
priority: low
ordinal: 1067000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Follow-up of TASK-1850 (closed for 2.0.0 on host and bench evidence with a field waiver, Robert A, DM 1573103087613124832). OT-Direct has no thermostat simulation (loopback simulates the boiler only; otThermostatSeen is set only by real thermostat frames, OTDirect.ino:1301/2457), so the loss and return transitions were only proven by tests/test_otd_thermostat_lines.py on the real checkThermostatTimeout(). This task checks them on an OT-Direct unit with a real thermostat on its bus.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 On an OT-Direct unit with a real thermostat, unplugging it gives one Thermostat disconnected line on port 25238 and as an OTGW event after the setback timeout
- [ ] #2 Replugging it gives one Thermostat connected line on port 25238 and as an OTGW event, with and without an engaged setback
- [ ] #3 Evidence (port 25238 transcript and telnet or event log with timestamps) recorded in the task
<!-- AC:END -->
