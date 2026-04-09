---
id: TASK-123
title: 'SAT Audit E2: ADR for SAT control architecture'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-08 21:55'
updated_date: '2026-04-09 05:24'
labels:
  - audit
  - sat
  - phase-5
  - adr
milestone: m-0
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Write an ADR for the SAT control architecture in the firmware: choice of embedded PID vs external, PWM vs continuous mode decision, and the overall SAT control loop structure.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 ADR describes context, alternatives and rationale for control architecture
- [x] #2 ADR published in backlog/decisions/ with correct next number
- [x] #3 Status: Proposed (for review)
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
ADR-062 (docs/adr/ADR-062-sat-smart-autotune-thermostat-integration.md) already fully documents the SAT control architecture: embedded C port rationale, alternatives considered (keep-in-HA, full-port, selective port), source file structure, control architecture (heating curve + PID + dual modes + cycle tracker), safety system, settings/state layout, and API surface. No new ADR created to avoid duplication. Task complete by reference to ADR-062.
<!-- SECTION:FINAL_SUMMARY:END -->
