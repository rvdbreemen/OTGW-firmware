---
id: TASK-2013
title: >-
  HA thermostat drops out of the Comfort preset to none on beta.360
  (indigo_light), capture pending
status: To Do
assignee: []
created_date: '2026-11-18T06:24:00Z'
updated_date: '2026-11-18T06:59:00Z'
labels:
  - bug
  - needs-info
  - sat
dependencies: []
priority: medium
ordinal: 1031000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Reported by indigo_light in #beta-testing 2026-11-17 19:20 UTC (https://discord.com/channels/812969634638725140/914498730001072149/1572330567478018109): on beta.360 the HA thermostat leaves Comfort and goes to preset none (21 C) for no obvious reason, about 12 h into a 24 h Windows capture with a nightly reboot planned. Robert asked for a full capture; indigo will post the logs on 2026-11-18. Not TASK-1771 (fixed in beta.184). Not analysed: no capture yet.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 The capture shows when and why the preset changed, cited
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-18 HYPOTHESIS (comms 92, not verified, no capture of indigo yet): with satfollowthermostat on, a confirmed thermostat MsgID 16 change becomes the SAT target and also clears an active preset to none (manual ch05 5.21, 'A thermostat change also clears an active preset'; SATcontrol.ino:2497 active = satIsEnabled() && bFollowThermostat). If indigo's thermostat (or its program) changes its own setpoint at the moment the preset drops, that would explain it. Check in his capture: sat/follow_thermostat_enable, SATFOLLOW telnet lines and MsgID 16 T frames at the drop time. Found while analysing marceld91d's capture (20261117-081252), where follow is off and MsgID 16 went 19.0 -> 17.0 without effect.
<!-- SECTION:NOTES:END -->
