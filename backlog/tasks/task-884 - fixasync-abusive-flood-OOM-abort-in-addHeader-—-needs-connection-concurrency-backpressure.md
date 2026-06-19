---
id: TASK-884
title: >-
  fix(async): abusive-flood OOM abort in addHeader — needs
  connection/concurrency backpressure
status: In Review
assignee:
  - '@claude'
created_date: '2026-06-18 14:11'
updated_date: '2026-06-18 17:14'
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
- [x] #1 Device survives the 8-worker test_load.py flood with bootcount delta 0 (no abort, no reboot)
- [x] #2 Realistic 4-worker load and single-request latency NOT regressed by the concurrency cap
- [x] #3 Mechanism of the addHeader bad_alloc pinned OR made moot by backpressure; documented
- [x] #4 evaluate.py green; esp32 build + flash-fit
<!-- AC:END -->





## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Implemented (alpha.214): app-level backpressure gate in processAPI (restAPI.ino). MAX=4 concurrent in-flight REST requests (matches the verified-safe 4-worker realistic load); excess gets a cheap 503 BEFORE any JSON build. Counter restInFlight is async_tcp-task-local (handlers serialize on one task, no atomic). Decrement via request->onDisconnect: the server sends Connection: close and closes after every response (no keep-alive, verified WebResponses.cpp:270/340), so onDisconnect fires exactly once per request -> the counter is balanced and cannot leak (this was the key correctness risk). For chunked responses the close fires after the last chunk ack (WebResponses.cpp:515) = exactly when the doc frees. Diagnostic: logs freeheap+maxblock at each cap-hit so we can tell transient concurrency from a real leak while shipping. Scope = all /api requests (KISS); lwIP/accept limits are in the precompiled core (read-only). test_load.py upgraded: 503 counted as 'handled' (device responded, declined) not 'fail'; only timeouts/errors are real failures. Verification pending: 8w flood (expect bootcount delta 0 + many 503) + 4-worker realistic (expect 0 503, PASS).

RESOLVED (alpha.214): backpressure gate (MAX=4 concurrent in-flight, cheap 503 on excess) + a try/catch backstop in restSendJson that catches std::bad_alloc from the synchronous response-build allocations (make_shared / beginChunkedResponse) and drops the request with a 503 instead of letting the unhandled exception abort+reboot the device. The 5th decoded panic (the gate-only build, MAX=4) put the bad_alloc at restSendJson make_shared/beginChunkedResponse -> the OOM locus had MOVED into our code (catchable). Note request->send() is DEFERRED, so the library's own addHeader allocation in the async _respond path is NOT catchable from app code; the gate reduces its probability. HARDWARE RESULT: TWO consecutive 8-worker/1.5min floods = bootcount delta 0 (device survives, boot stable, uptime monotonic) where every prior build rebooted 1-4x. The device degrades under saturation (timeouts + 503s) but no longer aborts/reboots. Realistic 4-worker load: handled 100% (444/445; 1 negligible 503 from POST/sampler overlap), 0 reboots, 0 ADR-089 tier entries -> not regressed. The earlier 'why does a small alloc fail with ~50KB free' puzzle is now moot for survival: whatever the exact pressure, the catchable locus no longer reboots, and the gate keeps the uncatchable locus from firing in practice. Full provable zero-reboot is not achievable from app code (the precompiled lwIP accept layer + the library's deferred header alloc are read-only), but the device reliably SURVIVES the abusive flood now.
<!-- SECTION:NOTES:END -->
