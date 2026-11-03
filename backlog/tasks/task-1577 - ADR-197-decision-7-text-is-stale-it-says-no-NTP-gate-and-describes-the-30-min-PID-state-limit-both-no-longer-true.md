---
id: TASK-1577
title: >-
  ADR-197 decision 7 text is stale: it says 'no NTP gate' and describes the 30
  min PID-state limit, both no longer true
status: Done
assignee:
  - '@claude'
created_date: '2026-11-03T01:01:00Z'
updated_date: '2026-11-03T12:09:00Z'
labels:
  - sat
  - adr
dependencies: []
priority: low
ordinal: 723000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found during TASK-1415 (2026-11-03). ADR-197 (Accepted) decision 7 states the PID state restore has no NTP gate and describes the 30 min age limit as current behaviour. The code had the NTP-gated deferred load before TASK-1415 (entry 140: wait up to 60 s for NTP), and TASK-1415 (beta.29) removed the 30 min limit (entry 74). An Accepted ADR is not edited in place: it needs a correction note or a superseding ADR, which the maintainer accepts.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 The maintainer chooses: a dated correction note on ADR-197 decision 7, or a small superseding ADR for the restore rule
- [ ] #2 The chosen record states the current rule: restore at any age (entry 74), deferred until NTP is set (entry 140), last error/last temperature/derivative time restored unless a live room reading came first
- [ ] #3 tests/test_adr_governance.py and adr-lint green
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-03 maintainer decision: correction note (not a superseding ADR). Added '## Correction 2026-11-03 (TASK-1577, maintainer)' to ADR-197: decision 7 implemented in beta.29, restore at any age, no NTP gate (entries 74, 140), evidence tests.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
ADR-197 carries a dated correction note that decision 7 is implemented (TASK-1415, beta.29): PID state restored at any age, no NTP gate, as decided in entries 74/140. No superseding ADR, by maintainer decision.
<!-- SECTION:FINAL_SUMMARY:END -->
