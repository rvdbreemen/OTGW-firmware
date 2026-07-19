---
id: TASK-1044
title: >-
  Cut UI poll rates: tick the clock locally from epoch, slow otmonitor to 2s and
  device status to 5s
status: To Do
assignee: []
created_date: '2026-07-19 21:53'
labels: []
dependencies: []
priority: high
ordinal: 163000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Follow-up on TASK-1043. That task capped the two UI-polled endpoints server-side; this one removes the reason the UI polls them so hard in the first place.

Measured on the martreides capture of 2026-07-19 (60 minutes, real boiler): 133 OpenTherm frames/min, 3515 Read-Ack/Write-Ack in the hour (~1/s of actual value updates), and the four fastest-moving values (RelModLevel, TrOverride, TSet, Tboiler) each update roughly once every 5 to 6 seconds. Everything else moves once every ~48 seconds. Polling otmonitor at 1 Hz therefore returns five to six identical payloads per real change.

The clock is the only thing in the UI that genuinely wanted 1 Hz, and it does not need the network for it. /api/v2/device/time already returns an `epoch` member alongside the formatted `dateTime`, so the browser can learn two offsets from a single response (clock skew against the browser, plus the device's timezone offset derived from the pair) and then tick locally. No timezone database in the browser, and a DST change is picked up at the next status poll.

Target rates per open page:
- /api/v2/otgw/otmonitor: client 2s, server window 1500ms
- /api/v2/device/time: client 5s, server window 4000ms, clock ticked locally at 1Hz
- /api/v2/device/info: unchanged, already throttled to about once a minute

That takes one open page from 121 to 43 requests per minute, a 65% reduction.

The server window is deliberately set below the client interval (about 75%). setInterval is not exact: background throttling and GC shift ticks, so a tick at 1990ms is normal, and a window of exactly 2000ms would hand 429s to a well-behaved client.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Clock display updates every second without a network request, derived from the epoch member
- [ ] #2 Device timezone is honoured without shipping a timezone database to the browser
- [ ] #3 otmonitor polls at 2s and device/time at 5s per open page
- [ ] #4 Server rate-limit windows sit below the client intervals so normal timer jitter does not trigger 429
- [ ] #5 Build passes and evaluator shows no new failures
- [ ] #6 Verified on hardware: clock runs smoothly, heap and mode fields still update, no 429 in the console under a single open tab
<!-- AC:END -->
