---
id: TASK-1766
title: >-
  Flaky web UI test: v2-navigation 'the chip shows Connection without the stored
  Values tab's fetch' fails about 1 in 3 runs
status: To Do
assignee: []
created_date: '2026-11-09T21:48:00Z'
labels:
  - tests
  - webui
  - flaky
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 864000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Seen 2026-11-09 during TASK-1765. v2-navigation.test.mjs alone on the harness from origin/dev before TASK-1765: 2 pass, 1 fail with 'FAIL: the chip shows Connection without the stored Values tab's fetch, montab unchanged (/api/v2/health,/api/v2/device/info,/api/v2/otdirect/overrides,/api/v2/otgw/otmonitor,/api/v2/device/info)'. With the TASK-1765 harness: the same assertion once in a 3-file run, and once (other run) 'the BLE discovery card appears again for the slow-device run' / 'with a slow GET settings (800 ms) the link still ends on Sensors with the roster card in view (null)'. Timing-dependent; not caused by TASK-1765.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Reproduce the failure rate over at least 10 single-file runs and name the race (which fetch lands when)
- [ ] #2 Fix the test or the UI so 10 of 10 runs pass
<!-- AC:END -->
