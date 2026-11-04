---
id: TASK-122
title: 'SAT Audit E1: Review existing ADRs for SAT relevance'
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
Review all existing ADRs in backlog/decisions/ for relevance to the SAT implementation. Identify ADRs that don't exist yet but are needed for SAT architecture decisions.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 All existing ADRs reviewed and SAT relevance determined
- [x] #2 List of missing SAT ADRs compiled
- [x] #3 Outdated/conflicting ADRs flagged for update
- [x] #4 Follow-up ADR tasks created with label 'audit-fix'
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Read all 68 existing ADRs and identify SAT-relevant ones
2. Check SAT source files for undocumented architectural decisions
3. Compile findings and create follow-up tasks
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Reviewed all 68 existing ADRs (ADR-001 through ADR-068). SAT-relevant ADRs:
- ADR-085: Main SAT integration architecture (already exists, comprehensive)
- ADR-061: Platform abstraction (relevant for ESP8266/ESP32 SAT code)
- ADR-063: OTGW32 hardware support
- ADR-004/ADR-053: Static buffer allocation (applies to SAT)
- ADR-009: PROGMEM (applies to SAT)
- ADR-006: MQTT pattern (topic structure basis for SAT)
- ADR-016: Command queue (SAT uses addCommandToQueue)
- ADR-051: Dual structs (settings.sat, state.sat)

Missing ADRs identified:
- PID v3 implementation details (no ADR, just brief mention in ADR-085)
- OPV calibration algorithm
- Heating curve formula rationale
- SAT memory allocation specifics
- SAT platform compatibility strategy
- SAT MQTT topic structure

No existing ADRs conflict with SAT. No outdated ADRs identified due to SAT.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Reviewed all 68 existing ADRs for SAT relevance.

Findings:
- ADR-085 covers overall SAT integration architecture
- ADR-061, 063, 064, 087 (renumbered from 065) cover OTGW32/platform topics
- ADR-004/049/053 cover memory/buffer strategy
- ADR-006 covers MQTT patterns
- ADR-016, 041, 051 cover command queue, HA discovery, dual structs

No existing ADRs conflict with SAT. No ADRs are outdated due to SAT.

Missing ADRs (created as tasks 123-129):
- ADR-069: SAT PID v3 implementation
- ADR-070: OPV calibration approach
- ADR-071: SAT memory allocation strategy
- ADR-072: Heating curve algorithm
- ADR-073: SAT platform compatibility
- ADR-074: SAT MQTT topic structure
<!-- SECTION:FINAL_SUMMARY:END -->
