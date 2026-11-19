---
id: TASK-2910
title: >-
  REST busy-503 path aborts the firmware on a starved heap (operator new in
  addHeader); close the connection without a response instead
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-19T09:39:00Z'
updated_date: '2026-11-19T15:58:00Z'
labels:
  - rest
  - heap
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 1052000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Panic on .61 (2.0.0-beta.388+7a99f02, 2026-11-19 10:24:05, task async_tcp, elf dc5d837a; dump in OTGW-field-data/bench-otgw32/20261119-task2026-388/). Decoded backtrace: processAPI restAPI.ino:2765 (restInFlight >= restEffectiveInflightCap(), cap 1 below maxblock 16000, restAPI.ino:55-64) -> sendApiBusy :189 -> sendApiError :174 -> webSend webServerCompat.h:340 -> webApplyHeaders :207 -> AsyncWebServerResponse::addHeader (WebResponses.cpp:182) -> std::list node operator new fails -> __cxa_allocate_exception fails -> std::terminate -> abort. The gate meant to protect against OOM allocates its own 503 with a Retry-After header. Present since 63f22a5e1 (TASK-1124). Field-reachable without the bench hook: a starved heap (BLE without PSRAM, the .61 beta.370 case) plus two simultaneous web requests. Coordinator decision 2026-11-19: fix in 2.0.0.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Host test calling the real sendApiBusy/sendApiError with an addHeader that fails like operator new: red on the old code (abort), green on the fix (connection closed, no response built)
- [x] #2 Above the threshold the 503 + Retry-After path is unchanged (same test)
- [x] #3 evaluate.py --quick no new FAIL; esp32-combo build
- [ ] #4 Bench .61: heap hold + 2 simultaneous REST requests, no panic, the second request sees a closed connection
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-19 implemented in wt-410 lane-busy503 (uncommitted until the 388 push; becomes beta.389): sendApiBusy() (restAPI.ino) closes the connection without a response when platformMaxFreeBlock() < REST_BUSY_MIN_MAXBLOCK (4096, the same starved definition as WIFI_STARVED_MAXBLOCK), through the new webAbortCurrentRequest() (webServerCompat.h: request->abort() + client()->abort() while a pcb is left, the pattern of the file-gate error path). Above the threshold unchanged (503 + Retry-After: 1). Why 4096 suffices: a failing operator new for a header list node (~100 B) means the largest block was then below ~200 B, far below 4096; 4096 is below the 16000 at which the in-flight cap already drops to 1. (The 3828 B banner value of 10:24:28 was AFTER the panic reboot, Up 0:00, so it says nothing about the panic moment.) Review (coordinator): webAbortCurrentRequest() itself has no String, log or new; AsyncWebServerRequest::abort() (WebRequest.cpp:1064-1071) only sets flags, _this.reset() and _client->abort(); AsyncClient::abort() (AsyncTCP.cpp:929) = _tcp_abort, after which _pcb is NULL so the second abort is a no-op; lwIP tcp_abort skips the RST when its pbuf cannot be allocated (no C++ throw). Known residual risk, not changed: processAPI calls RESTDebugTf() before sendApiBusy (REST debug flag only, off by default); telnet output can allocate there. Red/green: tests/test_rest_busy_starved_heap.py compiles the real sendApiError/sendApiBusy; red on origin ('HEADER Retry-After: 1' then 'ABORT operator new in addHeader'), green on the fix (2 passed).

Rebased onto origin 24679e271 (beta.388 pushed): patch identical; red against origin's restAPI.ino (OTGW_RESTAPI_INO): 1 failed (the starved case), green on the fix: 2 passed. evaluate.py --quick 88 passed 0 failed. Committed as beta.389. Next: esp32-combo build, bench .61 (hold + 2 parallel REST requests), full suite (shared code) during the bench.

2026-11-19 evidence: AC3 evaluate.py --quick 88 passed 0 failed on 931d1cdef; esp32-combo build 2.0.0-beta.389+931d1cd 'Build completed successfully!' (11:28-11:29), pushed 24679e271..931d1cdef, posted #alpha-testing 1572951641324261396; full suite on 931d1cdef 344/344: 339 PASS, 3 capture-tool flakes (TASK-2912, not 389), test_refresh_storm known (TASK-2041), 1 skip run in wt-2026 (PASS). AC4 NOT met as written: under the deep heap hold on .61 no TCP reached the device at all (09: 20/20 connect timeouts, no SYN-ACK), so the starved busy path (< 4096 B) cannot be driven on the bench; the 10:24:05 panic fell in a narrow window (connections accepted right after an AP reconnect, addHeader failing). Coordinator decision A (2026-11-19): the < 4096 branch is proven host-side on the real code (red on origin = the panic path, green on the fix); on hardware 09 showed regression PASS 31 min (bootcount 1061 unchanged, 0 panic, 31/31 200) and the unchanged above-threshold path (5x5 parallel curl -i: 12x 200 / 13x 503 with Retry-After: 1, Connection: close, no panic). The gap stays named: the < 4096 branch itself is not demonstrated on hardware. Observation (not 389): one MQTT disconnect 11:48:08 -> 42 s, explained by the LAN broker supervisor restarting a hung mosquitto (09). Status stays In Progress: AC4 needs either a waiver from Robert or a hardware reproduction (2.1.0 option: a prerelease fault-injection key for the busy path).
<!-- SECTION:NOTES:END -->
