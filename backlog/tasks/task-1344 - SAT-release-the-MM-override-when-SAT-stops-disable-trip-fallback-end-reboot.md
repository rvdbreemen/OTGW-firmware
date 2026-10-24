---
id: TASK-1344
title: >-
  SAT: release the MM override when SAT stops (disable, trip, fallback end,
  reboot)
status: To Do
assignee: []
created_date: '2026-10-24T10:10:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 467000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found by the SAT port audit 2026-10-24 (docs/sat/sat-port-audit-2026-10-24.md), finding #1. satDisable() (SATcontrol.ino:2328) only sends CS=0; no SAT path sends MM release, so MM=0 from PWM stays on the PIC (gateway.asm:4752-4765, single source) and the thermostat gets the boiler back at max modulation 0 %. thermo-nova serial coordinator sets MM on every update (coordinator/serial/__init__.py:228-233). Not a behaviour choice for SAT control itself (it is the hand-back), but name it to Sergeant D in the plan.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Every SAT stop path (disable, trip, MQTT-fallback end, boot with SAT off) releases MM (host test red on HEAD, green on fix; bench capture shows MM= release on PIC)
<!-- AC:END -->
