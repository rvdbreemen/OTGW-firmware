---
id: TASK-1947
title: >-
  Web UI fresh load sometimes gets a 503 from the ADR-188 connection cap while
  the browser has at most 2 requests open
status: To Do
assignee: []
created_date: '2026-11-14T20:18:00Z'
updated_date: '2026-11-14T21:16:00Z'
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
- [x] #1 Reproduce a 503 on a fresh load with the WEB CONN CAP telnet line captured (held and ws counts) and the CDP request timeline side by side
- [ ] #2 Explain which connections the server counts as holding TX at that moment, with evidence
- [ ] #3 Decide with evidence whether the cap counting or the UI load order needs a change; if a change: fresh loads show 0 webconn 503s over at least 20 loads (red before, green after)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-14 22:13 (09, .61 beta.252+941a224, idle, no other :80 clients): AC1 reproduced. Run A: 8 fresh loads (netload.mjs, 12 s each) with telnet: loads 3 and 6 got 10 and 7 503s, load 5 one; hd_webconn_503 20 -> 38. Telnet: 'WEB CONN CAP: 6 connections hold TX (0 ws, cap 4) => 503' (also 5 and 6 with 1 ws) at 22:07:50-22:08:54. CDP at the same time: never more than 2 requests open (peak /api/v2/settings + /favicon.ico).

Run B (22:12, 6 loads) with telnet and PC-side netstat sampled every ~1-2 s: load 4 got 14 503s, load 5 two. Device 22:12:37.1-22:12:40.3: 'WEB CONN CAP: 6 connections hold TX'. PC at 22:12:37.06 and 22:12:38.18: 1 ESTABLISHED (the WebSocket) and 14-15 TIME_WAIT to .61:80; max ESTABLISHED in the whole run 2. So the device counts 6 TX holders while the browser has closed all but one: the cap counts connections the client already closed.

Pointer for AC2 (read in code, not verified on the device): webConnCapExceeded() (restAPI.ino:85) -> WebQueuedFileResponse::txHolding() (webServerCompat.h:540) -> platformTcpTxHoldingOnPort() (libraries/Platform/src/platform_esp32.h:502) counts every pcb on port 80 in tcp_active_pcbs with unsent or unacked data regardless of state. tcp_active_pcbs also holds FIN_WAIT_1, CLOSING, LAST_ACK and CLOSE_WAIT pcbs, and in lwIP a sent FIN stays in unacked until it is acked, so closing connections after Connection: close probably count as TX holders. Next step: dump pcb->state for the counted pcbs at the 503 moment (telnet), then decide: skip states past ESTABLISHED, or count only pcbs with unsent payload. Evidence: 09 scratchpad r1947/ (tel.raw, netload-*.json, b/tel.raw, b/netstat.txt).

2026-11-14 22:20 (09): AC1 done by the bench. AC2/AC3 (pcb-state dump at the 503 moment, then the decision and fix with red/green over 20 loads) go to a developer; coordinator plans ba after its stack work. 09 benches the fix on request (netload.mjs + telnet + netstat recipe above).
<!-- SECTION:NOTES:END -->
