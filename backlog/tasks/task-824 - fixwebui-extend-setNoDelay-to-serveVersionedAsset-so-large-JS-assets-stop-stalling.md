---
id: TASK-824
title: >-
  fix(webui): extend setNoDelay to serveVersionedAsset so large JS assets stop
  stalling
status: Done
assignee:
  - '@claude'
created_date: '2026-06-05 18:18'
updated_date: '2026-06-05 18:29'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
alpha.162 cold-load capture: TASK-823 fixed index.html (10.5s/PENDING -> 1.5s) and CSS completes, but the 6 JS assets (sat.js 57KB, index.js, graph.js, etc.) still PENDING/never-finished. setNoDelay(true) was added only to the index.html path (sendIndex), not to serveVersionedAsset which streamFile()s the JS/CSS. Large streamFile transfers without setNoDelay stall on ~200ms delayed-ACK round-trips, amplified by serial-server contention of 6 parallel JS requests on weak WiFi. Add httpServer.client().setNoDelay(true) at the start of serveVersionedAsset (before streamFile).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 httpServer.client().setNoDelay(true) set at start of serveVersionedAsset before streamFile
- [ ] #2 Device verify: cold-load page-load completes ALL 8 assets (no PENDING), no watchdog reboot
- [ ] #3 build esp32+esp8266 green; evaluate.py --quick no new failures
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
REVERTED — NOT shipped. Premise falsified: validated that the ESP32 Arduino core already sets TCP_NODELAY on every accepted WebServer client (WebServer.cpp:71/77 + NetworkServer.cpp:65 setsockopt), so setNoDelay on serveVersionedAsset is a no-op on the OTGW32. Clean capture confirmed the 6 JS PENDING is NOT Nagle and NOT a reboot/watchdog (device stayed up 12min, served other REST 12-449ms throughout) — it is connection-acceptance/concurrency starvation on the serial single-connection WebServer with a second active browser client. Real fix = reduce parallel JS requests (bundle 6 JS -> 1) or structural concurrent serving, not setNoDelay. setNoDelay remains relevant only for the ESP8266/dev port (there the WebServer does not force noDelay). Reverted version bump back to alpha.162.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
REVERTED / not shipped. setNoDelay is a no-op on ESP32 (core already sets TCP_NODELAY at accept); JS-PENDING is concurrency/connection starvation on the serial WebServer, not Nagle. Real fix tracked as a bundling task. Kept relevant only for the ESP8266/dev port.
<!-- SECTION:FINAL_SUMMARY:END -->
