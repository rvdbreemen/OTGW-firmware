---
id: TASK-1796
title: >-
  SAT self-tune: phase-based signal (warm-up vs floating), Sergeant D proposal
  (Q-1787a)
status: To Do
assignee: []
created_date: '2026-11-11T06:21:00Z'
updated_date: '2026-11-11T08:05:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 890000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeant D 2026-11-11 06:18:39 UTC, #dev-sat-mqtt msg 1569959525715542148 (entry 302): use the warm-up overdamping data for the heating curve self-tune by comparing the warm-up phase with the floating phase inside the deadband, then decide the coefficient change. Conflicts with ADR-205 Must (curve integral as the only signal) and entries 163/171, so adopting it needs a superseding ADR. Waiting for his answer to Q-1787a (asked msg 1569959952854945914: A split clean hours into warm-up and floating and decide on the difference, recommended; B warm-up only; C other). Do not implement before the answer and the follow-up threshold questions are recorded as entries.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Sergeant D's answer to Q-1787a recorded as a numbered entry, and any follow-up question (thresholds, step) asked and recorded one at a time
- [ ] #2 Design written with thermo-nova and firmware citations, and a Proposed ADR that supersedes ADR-205 for the changed signal
- [ ] #3 Implementation with host tests that are red on the old tuner and green on the new one, and a simulation run per house type
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-11 06:28 UTC: on Robert's order all open Sergeant D questions were asked in one combined message (#dev-sat-mqtt msg 1569961879022084211): Q-1787a (repeated, phase-based signal), Q-1787b (ADR-205 tuner until then: keep / clamp 1.6-2.4 / off), Q-1771a (preset restore after reboot, thermo-nova climate.py:175-188), Q-1777g (last Greek labels). Rows in docs/sat/open-questions.md.

2026-11-11: Sergeant D answered Q-1787a at 06:28:28 UTC (entry 303): keep the 3-counted-day rule; raise only when the warm-up and the floating phase both say higher (AND rule, our interpretation). Asked Q-1787c (lowering mirrored?) msg 1569963485633323010 and Q-1787d (phase split at the 0.1 C deadband of thermo-nova const.py:16 / pid.py:187,:205; per-phase ADR-205 rule +-1.0 C, step 0.1) msg 1569963676771942532. Robert 2026-11-11: priority, aim for a new build. Next: answers to c/d -> entries -> Proposed ADR superseding ADR-205 for signal and decision -> implementation with red/green host tests and simulation -> build.

2026-11-11 06:35:27 UTC: Sergeant D 'Yes mirrored. So the answer is A' (reply to Q-1787c; entry 304). Q-1787c = A (lower only when both phases say lower). Q-1787d = A by our reading (deadband split 0.1 C per tick, per phase ADR-205 rule +-1.0 C / step 0.1, change only when both agree), confirmed to him in msg 1569964436796739676; he may still correct it. Implementer: otgw-firmware-d3 (wt-selftune).

2026-11-11 07:04-07:19 UTC: Q-1787e (one phase without data; advice B) and Q-1787f (overshoot counts as warm-up; advice A) asked; round 12 simulation posted (msg 1569974584600690750). Sergeant D asked for one question per message for tracking; all open questions re-posted separately (msgs 1569974667819618396 to 1569974694289870849).

2026-11-11: entries 305 (Q-1787e B, the phase with data decides alone), 306 (Q-1787f A, both sides of the deadband), 307 (Q-1787b A, the current tuner stays on). Entry numbers claimed by comms after checking that every tree ended at 304 (coordinator informed).
<!-- SECTION:NOTES:END -->
