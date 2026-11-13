---
id: TASK-1885
title: >-
  SAT REST: add actual flow temperature, flame state and sensor ages to
  /api/v2/sat/status, and plot real flow in the SAT chart
status: To Do
assignee: []
created_date: '2026-11-13T06:23:00Z'
labels:
  - webui
  - sat
  - rest
dependencies:
  - TASK-1879
priority: medium
ordinal: 958000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
UI proposals U7 and U8 from the VTherm UI review (docs/sat/vtherm-comparison-2026-11-13.md, section Web UI). Today the SAT loop chart labels heating_curve as Flow (v2.js:7170) and sat/status carries no actual boiler flow temperature or flame state outside simulation (sim_flow_temp, sim_flame_on only). Add flow_temp, return_temp (when valid), flame_on, current_modulation already exists, and room/outside temperature age in seconds to satSendStatusJSON (SATcontrol.ino:4266-4548, JsonEmit, ADR-146). UI: setpoint vs actual flow in one panel, heating curve kept separately labelled, modulation % as a filled area on a right axis with a flame band; sensor ages shown as plain minutes (U8). Fields are additive (API contract: additive only). Firmware commit needs its prerelease bump.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Host test of the status JSON contains flow_temp, flame_on and the two age fields with correct values from a fixture state; absent on the old code
- [ ] #2 Chart shows actual flow next to the setpoint and labels the heating curve as such (webui test on a fixture payload)
- [ ] #3 esp32-combo build green; evaluate --quick no new FAIL; bench run by otgw-bench-tester shows flow_temp/flame_on matching the OT data on a PIC and an OT-Direct unit
<!-- AC:END -->
