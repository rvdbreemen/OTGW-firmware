---
id: TASK-310
title: '[TEST-M3] Add Test Plan section to backlog task template and retrofit TASK-293'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-18 19:22'
updated_date: '2026-04-19 07:05'
labels:
  - testing
  - review-2026-04-18
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
297 backlog tasks, zero with Test Plan or ## Testing sections. High-risk firmware changes (OTDirect PS=1, MQTT discovery) have no human-verifiable checklist.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Backlog task template includes a ## Test Plan section stub
- [x] #2 TASK-293 retrofitted with PS=0/1/auto/SAT-PID-under-PS=1 checklist
- [x] #3 Creation guide updated to request test plans for firmware-behaviour changes
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
docs/guides/backlog-cli.md: new ## Test Plan section with a 3-5-bullet template and a concrete CLI example for appending it to any task. TASK-293 retrofitted via backlog task edit --append-notes with a 6-bullet test plan covering PS=0 baseline, PS=1 suppression, PS=1+SAT active, auto-leave-PS, a negative case, and a roll-back procedure. Creation guide points new tasks at the template.
<!-- SECTION:FINAL_SUMMARY:END -->
