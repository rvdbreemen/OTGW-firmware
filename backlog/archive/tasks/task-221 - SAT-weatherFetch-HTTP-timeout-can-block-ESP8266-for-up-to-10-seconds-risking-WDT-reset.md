---
id: TASK-221
title: >-
  SAT: weatherFetch HTTP timeout can block ESP8266 for up to 10 seconds, risking
  WDT reset
status: Done
assignee:
  - '@claude'
created_date: '2026-04-09 05:27'
updated_date: '2026-04-09 05:35'
labels:
  - audit-fix
  - sat
  - scheduling
dependencies: []
priority: high
---

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 weatherFetch() calls http.GET() with 10-second timeout, blocking cooperative scheduler for up to 10s on ESP8266
- [x] #2 ESP8266 WDT default fires at ~8s; feedWatchDog() is called AFTER http.end() which is too late
- [x] #3 Fix: reduce timeout to 5s (within WDT margin) or call feedWatchDog() via HTTPClient progress callback
- [ ] #4 Alternative: skip weatherFetch on ESP8266 if no yield mechanism available in HTTPClient, or document the risk and rely on WDT reset recovery
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Reduced HTTP timeout in weatherFetch() from 10s to 5s (SATweather.ino:130). The ESP8266 hardware WDT fires at ~8s; with a 10s timeout the device could reset on a slow Open-Meteo API response. 5s keeps us well within the WDT margin while still allowing reasonable network latency. No functional change to the weather fetch logic.
<!-- SECTION:FINAL_SUMMARY:END -->
