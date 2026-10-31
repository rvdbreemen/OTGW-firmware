---
id: TASK-1521
title: >-
  SAT instrumentation: log every SAT decision, boot phase and loop metric for
  field replay and simulation
status: In Progress
assignee:
  - '@claude'
created_date: '2026-10-31T09:40:00Z'
updated_date: '2026-10-31T11:37:00Z'
labels:
  - sat
  - instrumentation
  - alpha
dependencies: []
priority: high
ordinal: 677000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Maintainer request 2026-10-31. Field captures (gruijter TASK-1520) show the SAT log misses the why: boot gate waits (NTP, boiler reply, outside temperature), the first TC=, follow-thermostat record vs adopt vs echo, PWM enable/disable reasons, CH/CS decisions. Goal: every SAT decision and important moment leaves one structured line, so a capture can be replayed and fed to simulations (TASK-1519 rounds use field data). Two levels: (1) telnet SAT debug (SATDebugTf): full detail per control tick, machine-parseable key=value; (2) SAT narrate log (satNarrate_P/satNarratef_, the SAT log shown in the web UI): key algorithm moments only, plain language, so a user can follow what SAT did.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Inventory in task notes: every SAT decision point (boot decide, takeover, hand-back, outside missing/back, follow-thermostat record/adopt/echo, TC= push/refused, PWM enable/disable with the triggering rule, CH/CS/MM decisions, DHW start/end, safety/fault skips, mode switches) with file:line and whether it logs today
- [ ] #2 Telnet: one parseable line per control tick with the inputs and outputs (room, filtered room, target, outside + source, curve, P, I, D, pid, final CS, MM, CH, mode, PWM phase/duty, flame, flow, return, DHW) under a stable prefix and key=value format
- [ ] #3 Telnet: boot metrics (time to boot decision, to first boiler reply, to valid outside temp, to first CS and first TC=, restored-state verdict) logged once per boot
- [ ] #4 Narrate log: every decision in the inventory that changes boiler control or the target leaves one plain-language line with the reason (rule or entry number)
- [ ] #5 Host test or capture-parser test proves the tick line parses into a CSV a simulation script can read (scripts/ parser), red without the line format
- [ ] #6 No String class (ADR-180), PROGMEM strings, heap/loop-time impact measured on the bench (max_loop_gap_ms before/after)
- [ ] #7 Bench capture with the new lines attached as evidence; docs/sat/firmware-sat-spec.md or a log-format doc describes the fields
- [ ] #8 Debug toggle like the other telnet debug toggles: SAT logging can be switched on and off (telnet key + REST/settings where the others have it); default low volume (key decisions and boot metrics only), a separate high-volume level (per-tick key=value line and every decision detail) that is off by default and switchable at runtime without reboot
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-31 Robert: 'Make sure like other logging we can turn the SAT logging on and off, with a debug toggle. Default it should be low volume, but if you need to you should be able to have a high volume of logging available.' Mind: telnet debug keys are TOGGLES (memory reference_telnet_drops_under_mqtt_burst), and high volume can drop telnet lines under MQTT burst: the tick line must stay short.

Inventory (AC1): docs/sat/sat-instrumentation-inventory-2026-10-31.md (delegated read-only inventory, spot-checked). Log format: docs/sat/sat-log-format.md. Advisor review applied: raw room, osrc, rv, praw, dhw in SATTICK; SATBOOT phase=boiler/outside; reasons in mode narration at each caller (duplicate removed from satSetControlMode); TC push, follow adopt (via loop flag), cold cutoff and max clamp edge narrations. 19 host harnesses got stubs for the new macros/statics. Full tests: 75 files, only test_capture_tool failing mid-edit by the TASK-1523 agent (now Ran 28 OK twice); evaluate --quick Failed 0, 100 %.
<!-- SECTION:NOTES:END -->
