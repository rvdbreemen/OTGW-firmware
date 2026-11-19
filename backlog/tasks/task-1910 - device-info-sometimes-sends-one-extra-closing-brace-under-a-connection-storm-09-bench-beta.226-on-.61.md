---
id: TASK-1910
title: >-
  device/info sometimes sends one extra closing brace under a connection storm
  (09 bench, beta.226 on .61)
status: Done
assignee:
  - '@claude'
created_date: '2026-11-13T18:55:00Z'
updated_date: '2026-11-16T09:05:00Z'
labels:
  - bug
  - rest
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 976000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
09: during a 6-thread connection storm, GET /api/v2/device/info returned a complete valid object followed by one extra closing brace (2x open, 3x close), 10 times in the run, 0 in 300 calm calls (bodies in 09's scratchpad b226/reboot2/badjson1..9.bin). psram_free is a constant 0 on .61, so the live ESP.getFreePsram() read in the multi-pass closure (restAPI.ino:3446) is not the cause. A strict JSON client (HA, web UI) rejects such a body. Likely in restSendChunked / the multi-pass closure output path; near TASK-1899 (same handler).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Reproduce on a host test of the chunked multi-pass writer or on the bench with the storm script
- [x] #2 Root cause with file:line; fix makes every body valid JSON under the storm (0 of N bad), red on the old code
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Root cause (ba 2026-11-14, lane wt-ba-1916): sendDeviceInfoV2()'s restSendChunked() closure read three heap values live instead of from its snapshot: psram_free (ESP.getFreePsram()), internal_free (heap_caps_get_free_size(MALLOC_CAP_INTERNAL)) and internal_maxblk (heap_caps_get_largest_free_block), restAPI.ino ~3454-3457 before the fix. restSendChunked() (jsonChunked.h) re-runs the closure per TCP window and ends when a pass lands 0 bytes at index = bytes sent; its DETERMINISM CONTRACT requires identical bytes on every pass. Under 09's storm the internal heap moves between passes: when internal_free/maxblk gains a digit between the pass that sent the last bytes and the end-detect pass, that pass returns 1 byte, the last one: '}'. That is exactly 09's body (a complete object plus one '}', 2 open / 3 close). psram_free being 0 on .61 does not rule the closure out: the internal heap values are the moving ones. Same class, also fixed: handleDebugDump()'s closure passed millis() to satReportedLastCycleClass(), whose class can change width at a time boundary; it now uses a frozen snap->nowMs (no SAT behaviour change). Fix: the values are frozen into DeviceInfoSnap / DebugDumpSnap once per request. AC1: tests/test_rest_chunked_determinism.py part 1 drives the real RestChunkWindow (sliced from jsonChunked.h) through an AsyncChunkedResponse-like filler loop; an emit whose number gains a digit on the end-detect pass yields the good body + '}' (reproduced on the host); a stable emit yields the exact body. AC2: part 2 gates every restSendChunked() closure in restAPI.ino against live heap/millis reads: old (OTGW_CHUNK_REV=HEAD) FAIL with 4 hits (3 in device/info, millis in the debug dump), fix OK; related tests OK, evaluate --quick 0 FAIL. Open for AC2: 0 bad bodies under 09's storm script on the next build (bench). Limitation: the gate sees direct calls inside a closure, not a helper that reads the heap.

2026-11-16 09:26-10:00 bench 09 on .61 (OTGW32, no PSRAM), 2.0.0-beta.341+3cb72c3 (contains 8963adbee). (1) Storm only (tools/json_probe.py, 6 threads of HTTP GET + raw TCP connect/close, 1000 GET /api/v2/device/info): 458 complete 200 bodies, 0 that are not valid JSON; the rest 503 (467, ADR-188 cap), TimeoutError 6, URLError 69. (2) The original reproduction, tools/reboot_storm.py with K=6 (the beta.226 run that gave 10 bad bodies in 16 cycles): 12 cycles, then 4 more = 16 cycles, problem cycles 0, bad JSON bodies 0, every cycle bootcount +1 with lastreset 'Software reset (User reboot)', no crash. Side observation, not a body fault: after cycle 12 the script's own device/info read got no 200 for about 4 minutes (60 retries with a 4 s timeout) while the storm held the connections; the unit did not reboot (bootcount steady). That is the ADR-188/ADR-240 send cap refusing with 503 plus client timeouts under a 6-thread storm, the designed behaviour, not a malformed body. Evidence: 09 scratchpad closeout/storm1910/ (out.txt, reboot/out.txt + cycles, reboot2/out.txt).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
AC1: reproduced on the host: tests/test_rest_chunked_determinism.py part 1 drives the real RestChunkWindow; an emit whose number gains a digit on the end-detect pass yields the good body + '}' (09's beta.226 body). AC2: root cause restAPI.ino sendDeviceInfoV2() restSendChunked() closure read internal_free / internal_maxblk / psram_free live (and the debug dump millis()); fix 8963adbee freezes them per request; part 2 gate old FAIL (4 hits), fix OK. Bench on beta.341+3cb72c3 (.61): the original reboot_storm reproduction 16 cycles 0 bad bodies (was 10 in 16 on beta.226) plus 458 storm-only bodies 0 bad. Not covered: a helper that reads the heap inside a closure (the gate sees direct calls only).
<!-- SECTION:FINAL_SUMMARY:END -->
