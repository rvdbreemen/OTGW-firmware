---
id: TASK-823
title: >-
  perf(webui): batch index.html chunked streaming into a 1KB buffer + setNoDelay
  to fix 16s serve and watchdog reboots
status: In Progress
assignee:
  - '@claude'
created_date: '2026-06-05 17:44'
updated_date: '2026-06-05 18:00'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Isolated measurement on OTGW32 alpha.161: GET / returns valid HTML (39232 bytes, all 8 assets ?v=-injected) but body-complete takes 15.9s (~2.5KB/s) because sendIndex streams index.html line-by-line via ~2000 tiny chunked sendContent() writes, each a separate TCP segment hit by delayed-ACK. Under cold-load (8 parallel asset requests + serial WebServer) this trips the ESP32 Task Watchdog -> reboot (Boot: Task watchdog, Reboots: 3) -> page never finishes. The TASK-822 ?v= injection added ~5 sendContent/asset-line, worsening 10.5s->15.9s. Fix: accumulate output into a 1KB stack buffer (stack-local for re-entrancy safety per the existing sendIndex contract; 1KB safe on ESP8266 4KB CONT stack), flush in ~1KB chunks (40x fewer writes), feedWatchDog between flushes, and setNoDelay(true) on the serve socket to kill delayed-ACK. Inline the injection (emit-by-length, no buffer mutation). Target: GET / well under 1s isolated, no watchdog reboots under cold-load.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 sendIndex streams index.html via a >=1KB accumulation buffer flushed in chunks, not per-line; total sendContent() calls drop from ~2000 to ~40 for a 39KB index.html
- [ ] #2 httpServer.client().setNoDelay(true) set on the index serve path
- [ ] #3 ?v=<fsHash> injection preserved for all 8 assets (emit-by-length, no lineBuf mutation); served HTML byte-identical to pre-change except chunk boundaries
- [ ] #4 accumulation buffer is stack-local (re-entrancy-safe per existing sendIndex contract) and <=1KB so ESP8266 CONT stack is not overflowed even under re-entry
- [ ] #5 feedWatchDog() called between buffer flushes
- [ ] #6 Device verify (field): reflash, isolated GET / completes <1s, and a headless-Edge cold page-load completes all assets with no Task-watchdog reboot
- [ ] #7 build.py (esp32+esp8266) green; evaluate.py --quick no new failures
<!-- AC:END -->
