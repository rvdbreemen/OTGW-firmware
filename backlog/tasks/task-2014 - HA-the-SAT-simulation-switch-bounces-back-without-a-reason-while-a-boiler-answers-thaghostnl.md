---
id: TASK-2014
title: >-
  HA: the SAT simulation switch bounces back without a reason while a boiler
  answers (thaghostnl)
status: To Do
assignee: []
created_date: '2026-11-18T06:27:00Z'
labels:
  - ha
  - sat
  - needs-maintainer-ok
dependencies: []
priority: low
ordinal: 1032000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Reported by thaghostnl in #alpha-testing 2026-11-17 20:35 UTC (https://discord.com/channels/812969634638725140/1514720723980259460/1572349389857361982), beta.170, PIC 6.8, Remeha Avanta: the discovered SAT simulation switch jumps back to off within a second; telnet shows 'SAT-SIM: enable rejected, boiler present' (refusal by design, TASK-795). Suggestion: do not offer the switch while a boiler is present, or publish it unavailable or with a reason topic. Robert replied 'I fully agree with this feedback' (22:10 UTC, message 1572373470514258021).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 In HA the simulation switch is not offered as usable while a boiler answers, or shows why it refused
- [ ] #2 Host test or bench evidence on a unit with a boiler
<!-- AC:END -->
