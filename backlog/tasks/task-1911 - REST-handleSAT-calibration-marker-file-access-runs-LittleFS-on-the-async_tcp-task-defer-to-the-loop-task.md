---
id: TASK-1911
title: >-
  REST handleSAT calibration-marker file access runs LittleFS on the async_tcp
  task: defer to the loop task
status: Done
assignee:
  - '@claude'
created_date: '2026-11-13T19:16:00Z'
updated_date: '2026-11-16T09:23:00Z'
labels:
  - bug
  - stability
dependencies: []
priority: medium
ordinal: 977000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Follow-up of TASK-1899 (narrow scope, coordinator 2026-11-13). handleSAT() in restAPI.ino (the TASK-586 heating-curve calibration markers, GET/POST/DELETE on the markers file) calls LittleFS.exists/open/rename/remove on the async_tcp task: 9 calls, reads and writes. Same cross-core flash risk as the TASK-1865 and TASK-1899 watchdog cases, but user-triggered and rare, and it writes, so a cache does not fit: the file work has to move to the loop task (request flag or queue, answer when done or 202 plus a status read). The TASK-1899 gate tests/test_rest_no_littlefs_on_async_tcp.py allow-lists handleSAT with exactly 9 calls; this task removes that entry.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 No LittleFS call in handleSAT runs on async_tcp; the markers file is read and written on the loop task
- [x] #2 The TASK-1899 gate allow-list entry for handleSAT is removed and the gate stays green; red on the old code
- [x] #3 Host or source test on the real handler; bench by the bench-tester: add, list and delete markers from the web UI under a polling load, no watchdog reset
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Implemented (ba 2026-11-15, lane wt-ba-1916, coordinator choice: plain A, the TASK-1912 dallasLabels pattern). New satMarkers.ino ('// --- satMarkers' block): RAM copy of /sat_markers.json ('[{...}]', max 2560 B = the FS-OTA keep-list cap, 20 markers), loaded in setup() after loadDallasLabels(), mutex (REST takes it with 100 ms and answers 503 when busy), loopSatMarkers() from loopLittleFSStatus() writes temp + fsCommitTempFile() only when dirty, the lock covers the snapshot not the flash write (generation counter keeps an edit made during the write), a failed write keeps the file and retries after 30 s, flushSatMarkers() in doRestart(), FSexplorer upload/delete -> satMarkersFileChanged() -> reload on the loop task, a later POST wins. The code lives outside restAPI.ino because every function there runs on async_tcp (the TASK-1899 gate's premise). handleSAT's markers branch now only parses and validates, then works on the copy: no LittleFS call. Label sanitising now also replaces '{' and '}' (they would break the object scan); API, status codes and UI unchanged. RAM: 2 x 2561 B from PSRAM when present (platformPsramAllocOnce), otherwise from the internal heap, allocated once at boot: about 5 KB internal on a board without PSRAM; if the allocation fails the markers API answers 503 and the rest runs. AC1/AC2: tests/test_rest_no_littlefs_on_async_tcp.py allow-list entry for handleSAT removed: FAIL on the old restAPI.ino (9 calls), OK on the fix. AC3 host part: tests/test_sat_markers_ram_copy.py compiles the real block + the real markers branch against an in-memory LittleFS: 8/8 OK on the fix (no handler file access, boot load, read-your-writes, ids max+1, delete first/middle/last/only + 404, 400, 409 at 20, 503 on busy, failed write keeps file + 30 s retry, POST during the flash write neither blocked nor lost, FSexplorer reload + later POST wins); with the old handler (OTGW_MARKERS_REV=HEAD) 6 FAIL. 34 tests that slice FSexplorer/helperStuff/setup/doRestart OK (fixtures got stubs, the dallas wiring regex allows other *FileChanged calls next to its own); evaluate --quick 0 FAIL. Bench (AC3) open: add, list and delete markers from the web UI under a polling load, no watchdog reset; plus an FSexplorer upload of the file and a reboot.

2026-11-16 bench 09, both board types, REST /api/v2/sat/markers (the API the web UI uses) under a polling load of two threads (device/info, /api/v2/sat, /api/v2/sat/markers every ~0.2 s), then a FSexplorer upload (POST /upload, multipart) of /sat_markers.json, a /ReBoot and a cleanup. .63 (Classic, PSRAM), 2.0.0-beta.340+7c12dfd, 09:42: 5 adds (201, ids 1-5), list [1..5], 2 deletes (200), list [3,4,5]; /sat_markers.json on flash holds [3,4,5] (written by the loop task); upload 200, list unchanged; after the reboot bootcount 159 -> 160 (only the planned reboot), lastreset Software reset (no WDT), no crash fields, markers [3,4,5] with their labels. .61 (OTGW32, no PSRAM, internal-heap path), 2.0.0-beta.341+3cb72c3: first run 10:05 showed that under the two pollers 2 of 5 adds and 1 delete got 503 'Server busy: too many concurrent requests' (the ADR-188 cap, a clean refusal, no partial write), persistence and reboot OK (842 -> 843, Software reset, markers [1,2,3]); rerun 10:20 with a client that retries 503 (as the web UI's queue does): 5 adds, 2 deletes all succeeded (retries: POST 1, GET 2), file [3,4,5], upload 200, after the reboot 843 -> 844, Software reset, no crash, markers [3,4,5]. Cleanup on both: markers deleted, /sat_markers.json now '[]' (it did not exist before; same effective state). Evidence: 09 scratchpad closeout/markers/ (markers-192.168.88.63.log, markers-192.168.88.61.log, r2/markers-192.168.88.61.log, markers1911.py, markers1911r.py).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
AC1: handleSAT serves the markers from a RAM copy; the file is read and written on the loop task (fd1ee75b9). AC2: tests/test_rest_no_littlefs_on_async_tcp.py FAIL on the old restAPI.ino (9 calls), OK on the fix; the TASK-1899 allow-list entry is gone. AC3: host tests/test_sat_markers_ram_copy.py 8/8 (6 FAIL on the old handler); bench on .63 (PSRAM) and .61 (no PSRAM): add, list and delete under a two-thread polling load, FSexplorer upload of /sat_markers.json, reboot: no watchdog reset (Software reset, bootcount +1 for the planned reboot only), no crash fields, markers survive. On .61 the ADR-188 cap refuses some requests with 503 under that load; a retrying client completes all of them. Not covered: the web UI page itself in a browser (the REST calls it makes were driven directly).
<!-- SECTION:FINAL_SUMMARY:END -->
