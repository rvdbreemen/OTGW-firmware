---
id: TASK-1508
title: >-
  Replay honours recorded timing: replay otframes.log with the captured frame
  gaps instead of the fixed 750 ms interval (follow-up of TASK-1503)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-10-30T06:53:00Z'
updated_date: '2026-11-04T14:33:00Z'
labels:
  - replay capture tooling
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 664000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
TASK-1503 records otframes.log (host time, device timestamp, source letter, frame). The firmware replay (/api/v2/simulate, /otgw_simulation.log) reads bare frames at a fixed interval (status shows interval_ms 750; OTGW-Core.ino:4263, :4278), so a 1.0 s field cadence replays at 0.75 s and SAT timers that depend on real time drift. Bench 2026-10-30: a 75-frame capture with a 0.9995 s median gap replayed with interval_ms 750. Scope options to weigh in the plan: (a) firmware accepts an optional timestamp prefix per line and waits the recorded delta; (b) a host-side replayer streams frames with the recorded deltas (e.g. over the existing replay upload in chunks or a new endpoint); compare RAM/flash cost and accuracy. Keep the current bare format working.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Plan with the option chosen and why (firmware vs host), Proposed ADR via adr-kit if it changes a contract
- [ ] #2 A capture with mixed gaps replays with each frame within 50 ms of its recorded gap (measured, old fixed-interval run shown as the red case)
- [x] #3 Bare-frame files keep replaying as today (regression test)
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Robert 2026-11-04 (interactive): firmware reads times. otgw_simulation.log may carry the otframes.log timestamp per line; the replay waits the recorded gap (from the device timestamp column), bare-frame files keep the 750 ms interval. Proposed ADR via adr-kit for the file format. Test: host test on the real replay parser/scheduler (mixed gaps within 50 ms; old fixed-interval red), regression for bare frames; bench replay on .61 with telnet timestamps.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Triage 2026-11-02 (beta.5, origin/dev 6c41cc696) in code: no: OTGW-Core.ino:4327/4342 next frame = millis()+fixed 750 ms. Agent-doable, host testable.

2026-11-04 (sonnet agent; lead reran in wt-int4): firmware reads times (Robert). otframes.log lines (host TAB device_time TAB source TAB frame, or the last 3 columns) dispatch after the recorded device-time gap (clamped 0..60 s, midnight-wrap, drift-free anchor); bare lines keep 750 ms; '#' lines skipped. GET /api/v2/simulate shows timing timed|fixed. ADR-209 Proposed. tests/test_replay_timing.py 4 OK (old: 2 FAIL on the timed cases, bare-frame cases pass = regression), pm_chain 40, capture_otframes 6, adr_governance 15 OK, evaluate 0 failed. Also removed a duplicate ADR-208 row in docs/adr/README.md. AC2 bench pending. Tag beta.72.
<!-- SECTION:NOTES:END -->
