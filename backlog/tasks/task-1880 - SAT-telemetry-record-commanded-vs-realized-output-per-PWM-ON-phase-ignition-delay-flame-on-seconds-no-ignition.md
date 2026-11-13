---
id: TASK-1880
title: >-
  SAT telemetry: record commanded vs realized output per PWM ON phase (ignition
  delay, flame-on seconds, no-ignition)
status: To Do
assignee: []
created_date: '2026-11-13T06:13:00Z'
updated_date: '2026-11-13T06:23:00Z'
labels:
  - sat
  - telemetry
  - low-load
dependencies:
  - TASK-1879
priority: medium
ordinal: 953000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Lesson 1 of docs/sat/vtherm-comparison-2026-11-13.md (TASK-1879). VTherm learns on realized duty e_eff (cycle_tick_logic.py:137-148). SAT's cycle classifier records per flame cycle, not per PWM ON phase. Add, telemetry only (no control change), per PWM ON phase: commanded ON time, ignition delay (ON start to flame-on), realized flame-on seconds, and an ignited yes/no flag; expose the last N phases and counters via REST/MQTT and the telnet log. Direct evidence for Q-P2 (saturation exit while the boiler did not ignite, gruijter 2026-11-12). No String class (ADR-180), PROGMEM literals, flat MQTT topics (ADR-101).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Host test reproduces a PWM ON phase without ignition and shows ignited=false with the commanded ON time; red on the old code (field absent), green on the fix
- [ ] #2 REST /api/v2 and MQTT expose the per-phase record and counters; no control behaviour changes (existing SAT host tests unchanged and green)
- [ ] #3 Bench run by otgw-bench-tester on a PIC and an OT-Direct unit shows the record matching the flame bit in a capture
- [ ] #4 Web UI (U10): a PWM phase card in the Technical view shows commanded ON time, ignition delay and ignited yes/no for the last phases; webui test on a fixture payload
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-13: UI follow-up U10 from the VTherm UI review added as an AC.
<!-- SECTION:NOTES:END -->
