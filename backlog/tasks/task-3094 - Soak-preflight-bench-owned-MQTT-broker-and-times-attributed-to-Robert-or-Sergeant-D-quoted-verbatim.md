---
id: TASK-3094
title: >-
  Soak preflight: bench-owned MQTT broker, and times attributed to Robert or
  Sergeant D quoted verbatim
status: To Do
assignee: []
created_date: '2026-11-26T05:56:00Z'
labels:
  - docs
dependencies: []
ordinal: 1161000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Docs-only. (1) Soak preflight: mqttconnected=true against bench-owned otgw-bench-mqtt before the soak clock, counted PASS criterion >= 99 percent of health samples, switch rule (Robert DM 1575388089852108851). Evidence: overnight soak 0 of 247 samples connected (_retro/2026-11-26.md finding 3). (2) discord.md rule: attributed times, dates and numbers quoted verbatim with message id (Robert DM 1575388608725000225).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 agent-build-and-bench.md soak section and beta-release-2x Phase 3 carry the MQTT preflight and the 99 percent criterion
- [ ] #2 .claude/rules/discord.md carries the verbatim-quote rule
<!-- AC:END -->
