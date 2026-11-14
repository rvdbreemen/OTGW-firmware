---
id: TASK-1911
title: >-
  REST handleSAT calibration-marker file access runs LittleFS on the async_tcp
  task: defer to the loop task
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-13T19:16:00Z'
updated_date: '2026-11-14T23:16:00Z'
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
- [ ] #3 Host or source test on the real handler; bench by the bench-tester: add, list and delete markers from the web UI under a polling load, no watchdog reset
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Implemented (ba 2026-11-15, lane wt-ba-1916, coordinator choice: plain A, the TASK-1912 dallasLabels pattern). New satMarkers.ino ('// --- satMarkers' block): RAM copy of /sat_markers.json ('[{...}]', max 2560 B = the FS-OTA keep-list cap, 20 markers), loaded in setup() after loadDallasLabels(), mutex (REST takes it with 100 ms and answers 503 when busy), loopSatMarkers() from loopLittleFSStatus() writes temp + fsCommitTempFile() only when dirty, the lock covers the snapshot not the flash write (generation counter keeps an edit made during the write), a failed write keeps the file and retries after 30 s, flushSatMarkers() in doRestart(), FSexplorer upload/delete -> satMarkersFileChanged() -> reload on the loop task, a later POST wins. The code lives outside restAPI.ino because every function there runs on async_tcp (the TASK-1899 gate's premise). handleSAT's markers branch now only parses and validates, then works on the copy: no LittleFS call. Label sanitising now also replaces '{' and '}' (they would break the object scan); API, status codes and UI unchanged. RAM: 2 x 2561 B from PSRAM when present (platformPsramAllocOnce), otherwise from the internal heap, allocated once at boot: about 5 KB internal on a board without PSRAM; if the allocation fails the markers API answers 503 and the rest runs. AC1/AC2: tests/test_rest_no_littlefs_on_async_tcp.py allow-list entry for handleSAT removed: FAIL on the old restAPI.ino (9 calls), OK on the fix. AC3 host part: tests/test_sat_markers_ram_copy.py compiles the real block + the real markers branch against an in-memory LittleFS: 8/8 OK on the fix (no handler file access, boot load, read-your-writes, ids max+1, delete first/middle/last/only + 404, 400, 409 at 20, 503 on busy, failed write keeps file + 30 s retry, POST during the flash write neither blocked nor lost, FSexplorer reload + later POST wins); with the old handler (OTGW_MARKERS_REV=HEAD) 6 FAIL. 34 tests that slice FSexplorer/helperStuff/setup/doRestart OK (fixtures got stubs, the dallas wiring regex allows other *FileChanged calls next to its own); evaluate --quick 0 FAIL. Bench (AC3) open: add, list and delete markers from the web UI under a polling load, no watchdog reset; plus an FSexplorer upload of the file and a reboot.
<!-- SECTION:NOTES:END -->
