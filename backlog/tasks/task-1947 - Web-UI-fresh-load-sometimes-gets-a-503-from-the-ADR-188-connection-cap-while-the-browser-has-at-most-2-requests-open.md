---
id: TASK-1947
title: >-
  Web UI fresh load sometimes gets a 503 from the ADR-188 connection cap while
  the browser has at most 2 requests open
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-14T20:18:00Z'
updated_date: '2026-11-14T21:56:00Z'
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

Research 2026-11-14 (ba, origin/dev 753d45581). The counting is ADR-188 by design (Decision: 'A connection counts when it is ESTABLISHED, or in any other active state with unsent or unacked segments'); platformTcpTxHoldingOnPort() (src/libraries/Platform/src/platform_esp32.h:502) does exactly that, so a change needs an ADR-188 amendment and Robert's yes. Two paths can make the server count more than the browser has open: (a) every ESTABLISHED pcb on :80 without data that is not a native file-queue waiter counts, also a socket the browser opened ahead of a request (Chrome speculative preconnect, up to 6 per host) or a request whose headers are still arriving; (b) a closing pcb (FIN_WAIT_1, CLOSING, LAST_ACK) counts while only a zero-length FIN sits in unacked. ESPAsyncWebServer 3.11.2 sends 'Connection: close' (WebResponses.cpp:270, 340), so every request is a new socket that closes after one round trip. Host characterisation test tests/test_web_conn_cap_states.py (sliced real shim, stub pcb list): est_idle 1, est_waiter 0, est_data 1, close_wait 0, fin_wait_1 FIN-only 1, closing FIN-only 1, last_ack FIN-only 1, fin_wait_2 0, other port 0; a modelled fresh load (2 in flight + 2 idle preconnects + 2 FIN-only closing; a model, not a measurement) gives 6, matching 09's 'WEB CONN CAP: 6 connections hold TX'. Which path dominates on the device is not proven: next step (coordinator agreed) is a diagnostic breakdown of the WEB CONN CAP line (est-idle / est-data / closing-data counts + payload bytes, no behaviour change) in a build after the 1937/1850/1778 stack, 09 reproduces with CDP + telnet, then the A/B fix choice goes to Robert by DM.

Diagnostic breakdown (no behaviour change, ba 2026-11-14): platformTcpTxBreakdownOnPort() in platform_esp32.h splits the same count into est-idle / est-data / closing-data / closing-fin plus queued payload bytes; webConnCapExceeded() prints it on the refusal path only ('WEB CONN CAP: N connections hold TX (ws, cap) => 503 [est-idle a est-data b closing-data c closing-fin d, B queued]'). Host: tests/test_web_conn_cap_states.py 7/7 (breakdown sums to the count in every state; modelled fresh load 6 = 3,1,0,2, 2872 B); tests/test_web_file_scheduling.py PASS (fixture stub for the new shim, slice ends before it); evaluate --quick 0 FAIL. Build after the stack; 09 then reproduces on .61 with CDP + telnet (AC1/AC2).

Stack repair (ba 2026-11-14): when f75ffb1ab was cherry-picked onto the 1937 stack, resolving the version-banner conflicts with the upstream side for every conflicted file also dropped this commit's changes in restAPI.ino and webServerCompat.h (only the Platform shim and the tests landed; the tests passed because they slice the shim and stub the rest). Found by comparing every stacked fix commit with its original (1937, 1850, 1778 identical apart from build stamps; 1947 not). Restored both hunks byte for byte (diff vs 537ace5e5 empty), and tests/test_web_conn_cap_states.py now also checks that webConnCapExceeded() calls the breakdown and that txBreakdown() exists: FAIL on the broken stack, 8/8 OK after the repair. Committed as a fixup, squashed into the 1947 commit at the final rebase.
<!-- SECTION:NOTES:END -->
