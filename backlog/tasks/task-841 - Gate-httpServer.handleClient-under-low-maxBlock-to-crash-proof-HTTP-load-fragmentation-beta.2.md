---
id: TASK-841
title: >-
  Gate httpServer.handleClient() under low maxBlock to crash-proof HTTP-load
  fragmentation (beta.2)
status: To Do
assignee: []
created_date: '2026-06-07 16:19'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Zero-browser A/B (transcript-20260607-164812) proved HTTP request serving is the heap fragmenter + crash trigger: beta.1 with -SkipBrowserCapture ran 41 min, maxBlock floor 4408, zero crashes; with the browser hammering HTTP, maxBlock collapses to 264 and the device faults (memcpy to NULL, epc1=0x4000df64 excvaddr=0). 1.6.0 crossed an umm_malloc fragmentation cliff under accumulated concurrent churn during HTTP serving. Gate the normal handleClient() serving paths on maxBlock so the webserver backs off before fragmenting into the failing-alloc regime, instead of crashing. Do NOT gate the OTA-upload handleClient (must continue to process flash chunks).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 New canServeHttp() helper gates on ESP.getMaxFreeBlockSize() >= MQTT_PUBLISH_MIN_MAXBLOCK; getMaxFreeBlockSize() only walked when heap tier != HEALTHY
- [ ] #2 Normal serving handleClient() sites (OTGW-firmware.ino main loop, OTGW-Core.ino) are gated by canServeHttp(); the OTA-upload handleClient (processes flash chunks) is NOT gated
- [ ] #3 New iHttpFragSkips counter, surfaced on telnet logHeapStats and MQTT stats
- [ ] #4 python build.py --firmware exits 0 and evaluate.py --quick shows no new failures
<!-- AC:END -->
