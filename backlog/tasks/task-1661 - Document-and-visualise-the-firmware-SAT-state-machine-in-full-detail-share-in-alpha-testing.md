---
id: TASK-1661
title: >-
  Document and visualise the firmware SAT state machine in full detail; share in
  #alpha-testing
status: Done
assignee:
  - '@claude'
created_date: '2026-11-05T17:52:00Z'
updated_date: '2026-11-05T19:13:00Z'
labels:
  - sat
  - docs
  - agentic
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 785000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert 2026-11-05 (session otgw-firmware-54): document and visualise the SAT state machine of the firmware in full detail and share it in #alpha-testing. Describes what the 2.x firmware port does today (source rank 4 per .claude/rules/sat-ground-truth.md), every state, transition, guard and timer cited as firmware file:line; where a transition implements a Sergeant D entry or an ADR, name it. Not a statement of what SAT should do.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 docs/sat/sat-state-machine.md lists every SAT state variable with its states, transitions, guards and timers, each cited as firmware file:line
- [x] #2 Diagrams (mermaid in the doc, plus a rendered visual page) cover every state machine in the doc
- [x] #3 A second reader opened every citation and tried to refute it; the result is recorded
- [x] #4 Posted in #alpha-testing in plain English, without naming the OTGW32 board
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Three background agents extract the firmware state machines with file:line (control: activity/fallback, CONTINUOUS/PWM, PWM duty, cold state + CH enable, start-up timeline; cycles: phase, kind, classifier, flame status; boiler status + publication). 2. Assemble docs/sat/sat-state-machine.md with an overview diagram of how the machines connect, mermaid per machine, tables. 3. A separate reader agent opens every citation and tries to refute it; fix or drop what fails. 4. Rendered visual page (HTML) in docs/sat/. 5. Push when 6b's beta.99 freeze is lifted, then post a plain-English summary with the GitHub link in #alpha-testing (no OTGW32 name).
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-05: docs/sat/sat-state-machine.md (1011 lines, 12 machines M1-M12, overview, timelines, findings) and docs/sat/sat-state-machine.html (14 diagrams). Three extraction agents (control, cycles, boiler) cited firmware file:line; two separate second readers refuted: control ~52 claim groups confirmed, 12 imprecise, 1 wrong (30 s CS keepalive does not run while the 5 s resend runs, SATcontrol.ino:656/:671/:736); cycles+boiler 107 confirmed, 9 imprecise, 2 wrong (wrong line 5105 -> 7105; satCycleCountClass never called). All fixed. Own spot-check in this session: SATcycles.ino:991-1004 (class OVERSHOOT -> PWM, UNDERHEAT_PWM -> continuous), SATcontrol.ino:948 (boiler status display only), :6880 and :1590 (bModSuppressed cleared at flame-off edge and each tick), :656/:671/:736 confirmed. All 14 mermaid blocks valid in the Mermaid Chart validator; headless Edge render: 14 SVGs, no errors, no page-level horizontal scroll at 375 px; I set useMaxWidth false so wide diagrams stay readable and scroll in their box (screenshot checked).

Pushed with 6b's beta.99 push (2c7485842 on origin/dev, GitHub link returns 200). Posted in #alpha-testing: https://discord.com/channels/812969634638725140/1514720723980259460/1567980243346522214 (English, no board name).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
AC1: docs/sat/sat-state-machine.md, 12 firmware SAT state machines (M1-M12) with states, transitions, guards and timers cited as file:line. AC2: 14 mermaid diagrams, all valid in the Mermaid Chart validator; docs/sat/sat-state-machine.html renders them (headless Edge screenshot checked; wide diagrams scroll in their box). AC3: two second readers refuted every citation: control ~52 confirmed, 12 imprecise, 1 wrong; cycles+boiler 107 confirmed, 9 imprecise, 2 wrong; all fixed; own spot-check of 5 key claims in code. AC4: posted in #alpha-testing (link in notes). Not done: the code oddities in 'Findings worth a look' are observations, not yet filed as tasks.
<!-- SECTION:FINAL_SUMMARY:END -->
