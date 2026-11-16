---
id: TASK-2009
title: >-
  SAT simulator: a hydraulic multi-room home with TRVs (flow split, pump,
  bypass, pressure) for testing SAT behaviour
status: To Do
assignee: []
created_date: '2026-11-16T21:03:00Z'
labels:
  - needs-maintainer-ok
  - sat
  - simulation
milestone: 2.1.0
dependencies: []
ordinal: 1029000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert (#dev-sat-mqtt 1571991933646274721, 2026-11-16) asked about a simulated home: living room with the central thermostat (big, fast, within an hour), kitchen, three bedrooms, a bathroom at living-room temperature, TRVs on all radiators, other rooms 2-5 C lower, different thermal mass per room; Sergeant D added that new boilers have a PWM circulation pump and need a differential (bypass) valve. Today's built-in SAT simulator (SATcontrol.ino:6420-6540) has one boiler water mass, one room, scenario events and a zone add-on (TASK-798: zones ~2 C lower, faster, ideal TRV per zone) but no hydraulics: no flow split, pump, bypass, pressure, TRVs throttling flow or per-room thermal mass beyond one speed factor (answer 1571993603994026116). Candidate for 2.1.0; Robert decides scope.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Robert's go and the scope recorded
- [ ] #2 Design (rooms, thermal mass, emitters, TRV model, pump and bypass, pressure) agreed with Sergeant D, with the open-source simulators considered (project rule)
- [ ] #3 The simulated home reproduces at least one field capture (for example indigo_light's Vitodens) closely enough to be useful, shown by a side-by-side chart
<!-- AC:END -->
