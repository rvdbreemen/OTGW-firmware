---
id: TASK-1734
title: >-
  Water pressure 0.1 bar: flag as no real sensor at once instead of after 10
  minutes (tjfs)
status: To Do
assignee: []
created_date: '2026-11-08T15:28:00Z'
labels:
  - webui
  - needs-maintainer-ok
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 841000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
tjfs 2026-11-08 15:13 #alpha-testing (msg 1569007058538201262): 'I think you are being too conservative with the pressure. no true boiler pressure can be 0.1 bar.' TASK-1682 (beta.138) flags a constant 0.10 bar for 10 minutes as a probable pressure switch. Proposal: flag 0.1 bar (or <= 0.1) immediately. Tester request: Robert decides.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Robert decides the rule (immediate vs 10-minute window, threshold)
- [ ] #2 If changed: web UI flag follows the new rule, test red/green
<!-- AC:END -->
