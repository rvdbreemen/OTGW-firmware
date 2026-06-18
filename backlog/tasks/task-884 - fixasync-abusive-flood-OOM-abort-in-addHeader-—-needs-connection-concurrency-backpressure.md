---
id: TASK-884
title: >-
  fix(async): abusive-flood OOM abort in addHeader — needs
  connection/concurrency backpressure
status: To Do
assignee: []
created_date: '2026-06-18 14:11'
labels: []
dependencies: []
ordinal: 100000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
With the TASK-883 chunked-streaming fix (ADR-145) the under-load IDF Task-Watchdog reboot is GONE (0 watchdog panics where the buffered path produced all-watchdog), heap stays healthy, realistic 4-worker load passes clean. BUT an UNTHROTTLED 8-worker flood (near-DoS, not realistic) now aborts on an unhandled std::bad_alloc thrown from AsyncWebServerResponse::addHeader (operator new) -> std::terminate -> abort -> reboot (decoded panic, scripts/tests/_serialcap.py). EXACT allocation-failure cause is NOT pinned: 46 KB free and a 31 KB max-block at the time argue against simple heap exhaustion, so the mechanism is unresolved. This is the 4th distinct failure surfaced by escalating per-path optimisation (WDT-on-alloc-storm -> core-affinity -> pre-size -> chunked -> addHeader abort); the pattern says per-path tweaks won't win against the flood. The RIGHT category is backpressure: cap concurrent connections/requests (e.g. AsyncTCP/ESPAsyncWebServer max active connections, or a server-side concurrency gate) so the device never accepts more concurrent heavy requests than its ~95 KB heap can serve, and rejects/queues the rest. Realistic load is unaffected by such a cap. This is a fresh architectural decision (fix-attempt #4) for the maintainer; do NOT auto-continue per-path fixes. Repro: python scripts/tests/test_load.py --workers 8 --delay 0 --minutes 1.5 --host <ip> on alpha.213.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Device survives the 8-worker test_load.py flood with bootcount delta 0 (no abort, no reboot)
- [ ] #2 Realistic 4-worker load and single-request latency NOT regressed by the concurrency cap
- [ ] #3 Mechanism of the addHeader bad_alloc pinned OR made moot by backpressure; documented
- [ ] #4 evaluate.py green; esp32 build + flash-fit
<!-- AC:END -->
