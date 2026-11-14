---
id: TASK-1947
title: >-
  Web UI fresh load sometimes gets a 503 from the ADR-188 connection cap while
  the browser has at most 2 requests open
status: To Do
assignee: []
created_date: '2026-11-14T20:18:00Z'
labels:
  - webui
  - bug
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 995000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Measured by 09 on 2026-11-14 on .63 (Classic+S3, beta.250, idle, no other web clients known) with a headless Chrome CDP script (scratchpad tools/netload.mjs) that tracks Network.requestWillBeSent/loadingFinished per request. Per fresh load: 16-17 requests (14-15 /api/), maximum open at the same time 2 overall and 2 /api/ (peak: /api/v2/settings + /favicon.ico), so the UI keeps the 2-in-flight rule. Still 1 of 5 loads got a 503 on the first API call /api/v2/settings at t=2.07 s, right after GET / (204 KB, 2.04 s) finished; the UI retried and got 200 at 2.53 s. device/info hd_webconn_503 = 9 since boot on .63 (this counter includes earlier smoke loads that also showed 503s on settings, sensors/labels, sat/ble/discovery, otdirect/overrides, device/crashlog, debug, diag/recording/status, sat/weather, filesystem/hash-check; on .61 with beta.252 up to 13 per load when reloading quickly). Hypothesis (not verified): webConnCapExceeded() (restAPI.ino:85-95) counts connections that still hold TX data on the server side, e.g. the 204 KB index response still draining or closed-but-not-freed connections from the previous load, so the server sees more than WEB_MAX_TX_CONNECTIONS=4 while the browser sees 2. The WEB CONN CAP telnet line (held/ws counts) was not captured: two loads with telnet open gave no 503.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Reproduce a 503 on a fresh load with the WEB CONN CAP telnet line captured (held and ws counts) and the CDP request timeline side by side
- [ ] #2 Explain which connections the server counts as holding TX at that moment, with evidence
- [ ] #3 Decide with evidence whether the cap counting or the UI load order needs a change; if a change: fresh loads show 0 webconn 503s over at least 20 loads (red before, green after)
<!-- AC:END -->
