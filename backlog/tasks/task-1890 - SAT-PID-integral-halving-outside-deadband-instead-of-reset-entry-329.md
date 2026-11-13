---
id: TASK-1890
title: SAT PID integral halving outside deadband instead of reset (entry 329)
status: To Do
assignee: []
created_date: '2026-11-13T08:09:00Z'
labels:
  - sat
  - adr
milestone: 2.0.0
dependencies: []
ordinal: 963000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeant D 2026-11-13 (https://discord.com/channels/812969634638725140/1105556725714649128/1570710410427109397) accepted Robert's proposal (https://discord.com/channels/812969634638725140/1105556725714649128/1570705537383800905, https://discord.com/channels/812969634638725140/1105556725714649128/1570707806607245357): outside the deadband, halve the integral instead of resetting it to 0. Entry 329. Today thermo-nova sets the integral to 0 when |error| > DEADBAND 0.1 (pid.py:185-189, const.py:16) and the firmware does the same (_pidUpdateIntegral, SATpid.ino:247-250); halving is a deliberate difference from thermo-nova. Interpretation to confirm: multiply by 0.5 on each update while outside the band (decay), not once per crossing; the question goes with the PID research report the coordinator posts. Q-VT1 dropped (overtaken by entry 329). ADR-197 / ADR-204 (PID and its reset triggers) affected.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Plan with thermo-nova citations (pid.py:185-189, const.py:16) and SATpid.ino:247-250, plus Sergeant D's confirmation (decay per update vs once per crossing), before code
- [ ] #2 ADR that supersedes the integral-reset rule of ADR-197 (and checks ADR-204)
- [ ] #3 Host test on the real PID: red on reset-to-0, green on halving
- [ ] #4 Bench test by the bench-tester
<!-- AC:END -->
