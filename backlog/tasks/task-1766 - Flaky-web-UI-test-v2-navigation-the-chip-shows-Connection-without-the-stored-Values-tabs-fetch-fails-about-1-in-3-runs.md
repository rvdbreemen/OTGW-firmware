---
id: TASK-1766
title: >-
  Flaky web UI test: v2-navigation 'the chip shows Connection without the stored
  Values tab's fetch' fails about 1 in 3 runs
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-09T21:48:00Z'
updated_date: '2026-11-10T12:19:00Z'
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
- [x] #1 Reproduce the failure rate over at least 10 single-file runs and name the race (which fetch lands when)
- [ ] #2 Fix the test or the UI so 10 of 10 runs pass
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-10 (otgw-firmware-80 + an opus agent; details and run outputs in the session scratchpad final*.txt, instr-*.txt). Baseline: old test 5/10 single-file runs. Races named: (1) the fake device had no WebSocket: every failed connect -> ws.onclose -> fetchSeed() (otgw/otmonitor GET) + retry every 5 s (v2.js connectWs ~2776-2788, 20 s fallback ~8199): seeds landed in the 800 ms chip window ('chip shows Connection without the stored Values tab's fetch'). Fix: the fake device completes the WebSocket handshake and keeps it open (one WS per navigation, no reconnects, no seeds). (2) BLE discovery card: TASK-1741's TAB_PHASE_MS = Math.random()*15000 (v2.js:7528) puts the first BLE poll at 13.75 s + phase, up to 28.75 s, past the test's 25 s wait; red with the phase pinned at 0.999 (2/2), green at 0. Fix: the test pins Math.random to 0 via Page.addScriptToEvaluateOnNewDocument. Not a UI bug. (3) Chrome itself stalls on this loaded machine (requests held as 'sent' with 0 open server connections; Page.enable unanswered; also on the unchanged test): CDP calls now have deadlines, until() is wall-time bound, load/click failures are named, start-up gets one retry. (4) 'Advanced tab stored' never reproduced in ~27 instrumented runs; a stale-document marker was added (unproven).

Result: final test 7/10 single-file runs (batch 6); all 3 failures are named Chrome stalls (first page load not finished in 15 s twice, chip not up in 20 s), none traced to the test logic or v2.js. My own extra run: 1/1 pass, chrome count 0. AC2 (10/10) NOT met: blocked by the Chrome stall on this machine (cause not established; next capture: log every server request on arrival with CDP Network events, see final-instr.mjs). v2.js untouched.

2026-11-10 coordinator: stays In Progress; the remaining 3/10 failures are the Chrome request stalls on this machine, the same harness problem as TASK-1765 (CDP Chrome leak/stall). Resolve AC2 together with TASK-1765.
<!-- SECTION:NOTES:END -->
