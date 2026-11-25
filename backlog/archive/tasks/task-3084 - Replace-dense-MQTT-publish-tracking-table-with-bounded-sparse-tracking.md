---
id: TASK-26
title: Replace dense MQTT publish tracking table with bounded sparse tracking
status: To Do
assignee: []
created_date: '2026-03-19 17:04'
updated_date: '2026-03-19 18:04'
labels:
  - mqtt memory gating refactor
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Reduce the permanent 256-slot MQTT publish eligibility table to a bounded sparse structure that tracks only observed and gated message IDs. This can recover 512 to 768 bytes but is a higher-risk behavioral refactor because it changes the core eligibility bookkeeping for MQTT throttling and refresh.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 The dense 256-entry MQTT publish tracking table is replaced by a bounded sparse representation
- [ ] #2 Tracked first-seen, change-detect, stale-refresh, and reconnect semantics remain compliant with ADR-052
- [ ] #3 Status-byte and per-bit status publish tracking keep their current behavior
- [ ] #4 The implementation remains static-allocation based and does not introduce heap allocation in the hot path
- [ ] #5 Build succeeds and targeted regression testing validates publish gating behavior
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Superseded by explicit user direction.

Reason:
- TASK-26 proposed a bounded sparse tracking structure for MQTT publish eligibility.
- You later requested a simple linear 0..127 msgid array instead of a sparse or gap-compacted representation.
- The larger MQTT memory savings were delivered through publish streaming and buffer reduction, so this higher-risk refactor is intentionally not pursued.
<!-- SECTION:FINAL_SUMMARY:END -->
