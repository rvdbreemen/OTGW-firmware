---
id: TASK-1924
title: >-
  SAT: send CS= on change with a 30 s repeat, recompute the flow clamp every 5 s
  while the flame is on (entries 334-337)
status: To Do
assignee: []
created_date: '2026-11-14T07:58:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 986000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Maintainer order by Robert, #alpha-testing 2026-11-14: https://discord.com/channels/812969634638725140/1514720723980259460/1571061727917768815 . Repeat interval 30 s chosen by Robert (DM 1571063604466417735). Sergeant D agrees (entry 335, 1571063490989523060) and answered Q-336a B (entry 337, 1571066425966067824). Today: satSendCS() sends on every control tick and satCmdResend() repeats every 5 s (SATcontrol.ino:595, :673-683, :756-761; entries 44/237, ADR-203/207). Open points from entry 337 for the plan: the exact code condition for 'flow clamp active', and whether the PWM ON phase uses the same clamp; both go back to Sergeant D if the code does not settle them.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 CS= is sent when the sent value (0.1 resolution) changes, and an unchanged CS= is repeated every 30 s; the 5 s resend of an unchanged CS= is gone (entries 334, 335)
- [ ] #2 While the flame is on and the flow clamp of entry 112 is active, the clamped CS is recomputed every 5 s and sent when it changes (entry 337)
- [ ] #3 MM=, TP= and CH= keep their on-change plus 60 s repair cadence (entries 237, 239); PIC keepalive and the 3 s PWM suppression refresh are consistent with the new CS rule
- [ ] #4 A new ADR supersedes the CS part of ADR-207 (and ADR-203 where still in force), via adr-kit
- [ ] #5 Host tests call the real sender code and fail on the old code (5 s repeat, no 5 s clamp recompute) and pass on the fix
- [ ] #6 Bench (bench-tester): CS= count per minute in steady state and during a warm-up, and no PIC CS expiry over 30 min
<!-- AC:END -->
