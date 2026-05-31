---
id: TASK-431
title: >-
  Investigate: rapid WebUI page-refresh freezes the OTGW (1.4.2-beta), requires
  network drop to recover
status: Done
assignee:
  - '@copilot'
created_date: '2026-04-26 10:16'
updated_date: '2026-05-29 19:00'
labels:
  - bug
  - webui
  - 1.4.2-beta
  - needs-investigation
dependencies: []
references:
  - 'Discord #beta-testing, user andrebrait, 2026-04-23 21:24 UTC and 22:07 UTC'
  - >-
    Discord #beta-testing, user crashevans, 2026-04-23 18:24 UTC (possibly
    related browser slowdown)
  - 'Build: 1.4.2-beta+62fdacd'
  - >-
    docs/adr/ADR-089-heap-tier-machine-contract.md (heap counters useful for
    triage)
  - >-
    docs/adr/ADR-088-mqtt-status-burst-windowing-and-cooldown.md (related
    publish-side timing)
priority: high
ordinal: 7000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Reproducible firmware-side freeze reported by **andrebrait** in Discord `#beta-testing` on 2026-04-23 21:24 and 22:07 UTC, against build **1.4.2-beta+62fdacd**.

## Symptom

Refreshing the OTGW WebUI a handful of times in quick succession causes the device to freeze. The reporter quotes: "just refreshing it a handful of times, somewhat quickly, in succession, causes this. It's reproducible." Recovery requires asking the router (Unifi in this case) to drop the device's connection so the ESP can re-acquire its WiFi association. A normal browser-side reload is not enough.

The maintainer's reaction at 21:59 UTC: "wow, that's not good." That confirms this was not an expected behaviour and was not surfaced earlier in the 1.4.x beta cycle.

## Possibly related context

Earlier the same day (2026-04-23 18:24 UTC) **crashevans** reported "drastic browser slowdown" with several Safari errors visible in Web Inspector, including:

- `Failed to load resource: The network connection was lost. (index.js, line 0)`
- `ReferenceError: Can't find variable: initMainPage`
- `Unhandled Promise Rejection: TypeError: null is not an object (evaluating 'document.body.appendChild')`

Crashevans' issue resolved itself within ~30 minutes ("Must have been wifi connection") and the maintainer hypothesised "browser cache thingy". The andrebrait freeze a few hours later is consistent with the same underlying cause but reproducible enough that it is unlikely to be a one-off browser cache problem.

The 1.4.2-beta release notes (published a few minutes after crashevans' report) call out: "WiFi credentials no longer wiped on reboot (Core 3.1.2 gotcha with `WiFi.disconnect()` hitting NVRAM)" and "New bootrom-level reset path fixes the slow / frozen-post-boot state when upgrading from 1.3.x". So 1.4.2-beta has just touched WiFi reset and reboot paths; an interaction with rapid HTTP request bursts is plausible.

## Likely investigation paths

1. WebSocket lifecycle on rapid reconnects: a fresh page load opens a new WS and tears down the previous one. A burst of 3-5 reloads in a few seconds may exhaust the WS client slot pool or leave dangling connections that block the WiFi stack.
2. lwIP TCP socket pressure: each WebUI reload also reopens HTTP connections for index.html, index.css, index.js, graph.js, plus several REST `/api/v2/...` polls. The Arduino Core 3.1.2 lwIP stack has fewer free PCBs than 2.7.4; bursts may starve the pool.
3. Heap pressure under burst load: each request allocates response buffers; rapid bursts could push heap into CRITICAL tier (per ADR-089, `< 1536` bytes) and leave the device unable to accept new connections until the gate clears.
4. CONT stack overflow under deep call chains during burst handling.

## Reproduction recipe (to confirm before deep investigation)

1. Flash 1.4.2-beta+62fdacd to a Wemos D1 mini with full LittleFS image
2. Open WebUI in browser
3. Repeatedly reload (Cmd-R / Ctrl-R) 5+ times within ~3 seconds
4. Observe: WebUI becomes unresponsive, telnet may also time out, device only recovers after WiFi-side disconnect
5. Capture telnet log during the burst (stream to file before triggering the reloads)

## Information readiness

**Sufficient to start root-cause investigation.** Reporter is reachable, reproduction is concrete, hardware and build are known. Telnet logs from the freeze window would be the next concrete asset; if reproduction in maintainer's environment also triggers the freeze, those can be captured locally without waiting on the reporter.

## Out of scope

- Mobile header light-mode CSS overlap (separate issue, andrebrait at 21:22, treated as resolved per user feedback in this triage cycle).
- Soft-reboot WebUI reload delay (simontemplar6623 at 2026-04-24 10:22, treated as resolved or related-but-secondary; user excluded from triage scope).
- Upgrade firmware page UI flash-order instruction (separate, andrebrait at 20:45, treated as resolved per triage scope).
- ArnoudPJ's Wemos Boya bootloop (separate, tracked as TASK-430 from this session's earlier rescue).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Reproduce the freeze locally (or get a telnet log from the freeze window) on 1.4.2-beta+62fdacd or later. Confirm the recipe: 5+ rapid reloads within ~3 seconds, freeze, recovery only after WiFi-side disconnect
- [ ] #2 Identify root cause: WebSocket slot exhaustion vs lwIP PCB starvation vs heap-tier CRITICAL transition vs CONT stack overflow vs other. Anchor the diagnosis to telnet log evidence and (if applicable) heap-tier counters (ADR-089: iEnteredWarningCount, iEnteredCriticalCount)
- [ ] #3 If a firmware fix is needed, implement it without violating ADR-088 (status-burst windowing), ADR-089 (heap tier-machine), or ADR-090 (re-entrancy guard pattern). If the fix touches publishing or scratch state, the relevant CI gates must continue to pass
- [ ] #4 Validate the fix with the same reproduction recipe; report negative result (cannot reproduce after fix) over at least 20 rapid reload cycles
- [ ] #5 Update Discord #beta-testing thread (andrebrait, sergeantd, crashevans) with status and ask for re-test on a fresh build
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
- Confirmed the same browser lifecycle bug is still present on the 2.0.0 branch: hidden tabs keep the OT log WebSocket alive, but reload/unload did not explicitly close it.
- Ported the dev mitigation into data/index.js: pagehide/beforeunload now persist the OT log buffer, stop polling timers, cancel delayed reconnects, and close the live OT log WebSocket; main-page reconnects now wait 250 ms so the previous page can retire its socket first.
- Added low-noise firmware-side burst diagnostics in webSocketStuff.ino so the next telnet retest can distinguish connect/disconnect churn from max-client, low-heap, or error bursts.
- Validation: ./build.sh and .build-venv/bin/python evaluate.py --quick both passed on the 2.0.0 worktree.
- Still blocked on real hardware/telnet evidence for AC #1/#2/#4 and reporter follow-up for AC #5.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Closed without full root-cause verification: the original reporter (andrebrait) never provided the hardware/telnet evidence needed to confirm the freeze, so AC#2 (root cause) and AC#4 (reporter validation) cannot be completed. A mitigation for the web UI rapid-refresh freeze shipped on 2.0.0 (commit ca845dd0) and the related WebSocket reconnect dedup landed under TASK-563 (commit 0cf287c5). No further freeze reports have come in. Closing per maintainer decision (no reporter feedback to validate against); reopen with a fresh repro + telnet trace if the freeze recurs on current builds.
<!-- SECTION:FINAL_SUMMARY:END -->
