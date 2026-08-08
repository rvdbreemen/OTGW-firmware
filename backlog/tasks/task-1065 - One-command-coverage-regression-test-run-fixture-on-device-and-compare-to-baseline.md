---
id: TASK-1065
title: >-
  One-command coverage regression test: run fixture on device and compare to
  baseline
status: To Do
assignee: []
created_date: '2026-08-08 12:25'
labels:
  - test
  - tooling
dependencies: []
priority: medium
ordinal: 175000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
TASK-1063 produced a baseline and a compare mode, but running the test is still a manual sequence: upload the fixture, POST simulate/start, capture telnet:23 for long enough, POST simulate/stop, then run compare. That is five steps to remember and several ways to get it subtly wrong (capture cut mid-loop reports false MISSING topics; forgetting to stop the simulation leaves the bench publishing synthetic data to a real broker). Wrap it in a single runner that takes a host and does the whole thing, exits 0 on match and non-zero on drift, so it can be used as a gate after any firmware change.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 A single command runs the whole cycle against a device: upload fixture, start simulation, capture, stop simulation, compare to baseline
- [ ] #2 The simulation is stopped even when the capture fails or the comparison finds drift, so the bench is never left publishing synthetic data
- [ ] #3 Capture duration defaults to at least one full fixture loop and the runner refuses a duration that would cut a loop short
- [ ] #4 Exit code is 0 on a clean match and non-zero on drift or on any device error
- [ ] #5 The raw capture is written to a file so a drift can be investigated afterwards
- [ ] #6 Proven end to end against otgw1.local: a clean run reports PASS
- [ ] #7 scripts/tests/README.md documents the one-command form
<!-- AC:END -->
