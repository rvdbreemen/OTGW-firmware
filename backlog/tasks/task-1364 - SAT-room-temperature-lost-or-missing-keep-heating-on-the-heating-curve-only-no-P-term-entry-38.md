---
id: TASK-1364
title: >-
  SAT: room temperature lost or missing - keep heating on the heating curve
  only, no P term (entry 38)
status: To Do
assignee: []
created_date: '2026-10-24T21:41:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 499000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeant D 2026-10-24 (Q6, entry 38): keep heating on the heating curve; no proportional term without a valid room temperature. Replaces the firmware trip after ~10 ticks (audit #4). Plan quotes entry 38 and thermo-nova; post to Sergeant D as a notice.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 With no valid room temperature SAT keeps heating on the curve without P (host test red on HEAD: trips; green: curve only); stale-time rule documented
<!-- AC:END -->
