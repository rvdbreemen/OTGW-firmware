---
id: TASK-841
title: >-
  Gate httpServer.handleClient() under low maxBlock to crash-proof HTTP-load
  fragmentation (beta.2)
status: Done
assignee:
  - '@claude'
created_date: '2026-06-07 16:19'
updated_date: '2026-06-07 16:54'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Zero-browser A/B (transcript-20260607-164812) proved HTTP request serving is the heap fragmenter + crash trigger: beta.1 with -SkipBrowserCapture ran 41 min, maxBlock floor 4408, zero crashes; with the browser hammering HTTP, maxBlock collapses to 264 and the device faults (memcpy to NULL, epc1=0x4000df64 excvaddr=0). 1.6.0 crossed an umm_malloc fragmentation cliff under accumulated concurrent churn during HTTP serving. Gate the normal handleClient() serving paths on maxBlock so the webserver backs off before fragmenting into the failing-alloc regime, instead of crashing. Do NOT gate the OTA-upload handleClient (must continue to process flash chunks).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 New canServeHttp() helper gates on ESP.getMaxFreeBlockSize() >= MQTT_PUBLISH_MIN_MAXBLOCK; getMaxFreeBlockSize() only walked when heap tier != HEALTHY
- [x] #2 Normal serving handleClient() sites (OTGW-firmware.ino main loop, OTGW-Core.ino) are gated by canServeHttp(); the OTA-upload handleClient (processes flash chunks) is NOT gated
- [x] #3 New iHttpFragSkips counter, surfaced on telnet logHeapStats and MQTT stats
- [x] #4 python build.py --firmware exits 0 and evaluate.py --quick shows no new failures
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
AC#2 wording correction: OTGW-Core.ino:553 turned out to be the PIC-flash upload handler (handlePicFlashBackgroundTasks), not a normal-serving site. Per the AC's intent (gate normal serving, never gate flash-upload), it was left UNGATED alongside the ESP-flash handler. Only OTGW-firmware.ino:422 (main-loop serve) is gated. AC checked on intent, not the literal file list.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Added canServeHttp() maxBlock gate on the steady-state HTTP serving path (commit e6bec34c, pushed origin/dev). Crash-proofs the confirmed HTTP-load fragmentation: the zero-browser A/B (transcript-20260607-164812) showed beta.1 stable for 41 min without HTTP (maxBlock floor 4408) but collapsing to ~264 and faulting under browser load (memcpy to NULL, epc1 4000df64 excvaddr 0).

Implementation: canServeHttp() returns true when heap is HEALTHY (no free-list walk); otherwise skips handleClient() when getMaxFreeBlockSize() is below MQTT_PUBLISH_MIN_MAXBLOCK (1536), letting the heap coalesce while clients retry. Gated ONLY the main-loop serve (OTGW-firmware.ino:422). The OTA and PIC flash-upload handlers are deliberately left ungated so an in-progress flash completes. New iHttpFragSkips counter on telnet logHeapStats (HTTP_fragskips=) and MQTT stats (stats/http_fragskips).

Verified: build.py --firmware exit 0; evaluate.py --quick 34 passed, 0 failed.

Field validation pending (real efficacy gate): George flashes beta.2, keeps the web UI/dashboard open to recreate HTTP load, expect HTTP_fragskips to climb instead of Exception reboots, maxBlock floor to hold, 0 crashes over 24h. Degraded-but-alive trade: under heavy HTTP + low heap the UI briefly refuses/slows rather than rebooting. Deeper root (lean HTTP rewrite) is 2.0.0 scope.
<!-- SECTION:FINAL_SUMMARY:END -->
