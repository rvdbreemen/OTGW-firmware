---
id: TASK-1666
title: Diagnostic recording download and status REST routes (ADR-213 step 2)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-05T20:34:00Z'
updated_date: '2026-11-14T15:27:00Z'
labels:
  - diagnostics
milestone: 2.0.0
dependencies:
  - TASK-1470
priority: medium
ordinal: 789000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
GET /api/v2/diag/recording?since= as an octet-stream filler with the block range fixed at request start (like ADR-210), plus /api/v2/diag/recording/status (state, ring size, oldest/newest block seq, refusal reason). Open routes (Robert 2026-11-05). Counts as one of the ADR-188 send-holding connections; respects the 2-in-flight web UI rule.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Host test of the route wiring and the range-fixed-at-start rule
- [ ] #2 Bench: 1.5 MB download time and that the web UI stays responsive during it
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-06 implemented (sonnet subagent, reviewed): routes GET /api/v2/diag/recording[?since=N] (octet-stream, range fixed at request start, header OTGWREC v1: version, block size, first_seq, count, ring, boot, flags, route names), GET .../status, POST .../start|stop. Host test test/host/test_diag_recording_routes.py --mutant RESULT PASS (W1-W4, R1-R8; range mutant fails R1/R6, since mutant fails R2). ESP32 build OK. Bench .61 (no PSRAM): start ok, download 200 4300 B in 0.155 s with 5 concurrent device/info polls 200 in 0.09-0.18 s, stop ok. AC2 (1.5 MB download time) needs the PSRAM board .63 (after 2026-11-07 08:00). Open: the ADR-214 topic/setting name sections in the file header are not in this version (header format v1 has route names only).

Stale audit 2026-11-14 (coordinator decision): AC2 needs a bench on .63 (PSRAM) by 09; the coordinator queues it.
<!-- SECTION:NOTES:END -->
