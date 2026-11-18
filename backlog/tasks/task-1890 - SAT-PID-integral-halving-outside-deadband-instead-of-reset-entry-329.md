---
id: TASK-1890
title: SAT PID integral halving outside deadband instead of reset (entry 329)
status: To Do
assignee: []
created_date: '2026-11-13T08:09:00Z'
updated_date: '2026-11-18T14:26:00Z'
labels:
  - sat
  - adr
milestone: 2.1.0
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

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-13: after the I/D simulation (1570758325740507237) Sergeant D said 'The integral works pretty good. I am also happy about the derivative' (1570765446636900415), recorded as entry 332 (assessment, no change decided). It does not revoke entry 329; whether the halving is still wanted goes into the coordinator's PID report.

2026-11-13: Q-329a answered B by Robert (13:00:35 UTC, 1570785451613225121), recorded as entry 333 (maintainer answer, reading posted 1570786623585521665, no correction): halve the integral once when the error leaves the deadband, then hold it while outside; accumulate from the held value when back inside; target change keeps the full reset. Open: zone PIDs, exit right after a reset, the faster self-tune part (PID study).

2026-11-16: moved to milestone 2.1.0. Release scope per entry 386 (Robert 08:47:36 UTC, https://discord.com/channels/812969634638725140/1105556725714649128/1571808950330986547: 'Mark anything new to the next release'): the initial SAT release covers the decisions of entries 375-385; this item is outside that scope. Not built now (coordinator 2026-11-16).

2026-11-18 Q-329b queued for Sergeant D (docs/sat/open-questions.md; comms 92): halving for the main PID only, or for the zones too (entry 333 open point 2; thermo-nova pid.py:186-188 resets to 0, area.py:50 zones use the same PID). Until he answers: option B (main PID and zones) decided by directive (Robert, 15 Nov: decide and proceed, he may overrule), so the work can proceed.
<!-- SECTION:NOTES:END -->
