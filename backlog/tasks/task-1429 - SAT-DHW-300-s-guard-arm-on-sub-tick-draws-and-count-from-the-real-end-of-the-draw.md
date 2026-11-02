---
id: TASK-1429
title: >-
  SAT DHW 300 s guard: arm on sub-tick draws and count from the real end of the
  draw
status: To Do
assignee: []
created_date: '2026-10-28T18:16:00Z'
updated_date: '2026-11-02T04:38:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 593000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found in TASK-1420 review. satCycleNoteDhwTick() arms the guard only on control ticks that see the boiler DHW-active bit: a draw shorter than one tick (seen only by satPwmNoteDhwFlameEdge) never arms it, and the guard counts from the last DHW tick instead of the real end of the draw, so it can lift up to one interval earlier than thermo-nova hot_water_off_since + 300 s (heating_control.py:272-274, :313-315; entry 108).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Host test: a draw between two ticks arms the guard; the guard ends 300 s after the DHW bit dropped; red on HEAD, green on fix
- [ ] #2 build + evaluate green
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Triage 2026-11-02 (beta.5, origin/dev 6c41cc696) in code: no: guard only armed by satCycleNoteDhwTick (SATcycles.ino:212), flame-edge path (SATcontrol.ino:1117) does not. Entry 108 decided. Agent-doable.
<!-- SECTION:NOTES:END -->
