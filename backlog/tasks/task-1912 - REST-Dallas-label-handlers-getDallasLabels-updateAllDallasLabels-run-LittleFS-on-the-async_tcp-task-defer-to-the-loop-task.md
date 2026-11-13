---
id: TASK-1912
title: >-
  REST Dallas label handlers (getDallasLabels, updateAllDallasLabels) run
  LittleFS on the async_tcp task: defer to the loop task
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-13T19:17:00Z'
updated_date: '2026-11-14T02:24:00Z'
labels:
  - bug
  - stability
dependencies: []
priority: medium
ordinal: 978000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Follow-up of TASK-1899 (narrow scope, coordinator 2026-11-13). getDallasLabels() (GET, LittleFS.exists plus the file read) and updateAllDallasLabels() (LittleFS.open for write of /dallas_labels.ini) in restAPI.ino run on the async_tcp task. User-triggered and rare, with a write, so the file work has to move to the loop task rather than be cached. The TASK-1899 gate tests/test_rest_no_littlefs_on_async_tcp.py allow-lists both with exactly 1 call each; this task removes those entries.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 No LittleFS call in the Dallas label handlers runs on async_tcp; /dallas_labels.ini is read and written on the loop task
- [x] #2 The TASK-1899 gate allow-list entries for getDallasLabels and updateAllDallasLabels are removed and the gate stays green; red on the old code
- [ ] #3 Host or source test on the real handlers; bench by the bench-tester: read and save sensor labels from the web UI, no watchdog reset
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Approved by the coordinator 2026-11-13 (RAM copy instead of a plain deferred write). A static 2049 B RAM copy of /dallas_labels.ini in sensors_ext.ino, owned by the loop task and guarded by a PlatformMutex: loaded at boot before the web server, served by GET, replaced by POST/PUT (validation unchanged, still 200 {success:true}, no API change), written by loopDallasLabels() through a temp file and rename (power-cut safe, never while flashing, 30 s retry on failure), flushed by doRestart() like the settings. ensureSensorDefaultLabels() flushes a pending edit first, edits the file, and reloads the copy, all under the lock. REST takes the lock with 100 ms and answers 503 when busy; v2.js fetchWithRetry retries 503 (3 tries).
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Why (scope note, coordinator 2026-11-13): the case for this change is the lost update plus the small-file write path, not a rule that LittleFS is never read on async_tcp. Every static web file (v2.html.gz and the rest) is read by AsyncFileResponse on async_tcp under the ADR-212 file queue, and that stays the accepted pattern. What changed here: the labels POST wrote the flash file on async_tcp, and a plain deferred write would have opened a lost-update window for the web UI's read-merge-write (v2.js saveDallasLabel). The old code itself had no lost update, because its POST wrote synchronously before answering.

Evidence (2026-11-13, wt-95w lane-95-1912 on lane-95-1899): AC2 gate tests/test_rest_no_littlefs_on_async_tcp.py with both Dallas entries removed: on the old restAPI.ino exit 1 ('getDallasLabels: 1 LittleFS call(s), 0 allowed (4632 LittleFS.exists)', 'updateAllDallasLabels: 1 LittleFS call(s), 0 allowed (4692 LittleFS.open)'), on the fix 4 OK. AC1 tests/test_dallas_labels_ram_copy.py compiles the real dallasLabels block and the real handlers against an in-memory LittleFS: 9 OK. It covers read-your-writes before any flush, 0 file operations in either handler, temp + rename only when changed, a failed rename that keeps the old file and the edit and retries only after 30 s, 503 on a busy lock, 400 on a bad body, and boot load falling back to {} for a missing, garbage or oversize file. Lost-update guard: the mutant with the GET from before this task and the new POST returns the old labels after a POST (get_after_post = A). On the old sources the test fails (block not found, wiring regex). Also OK: test_rest_fs_flags_loop 7, test_health_no_fs_flash_ops 4 (stub for loopDallasLabels), test_fs_update_keep 38, test_board_pin_reserved 5, test_setting_gpio_range 5, test_ino_prototype_hoisting 2, test/host/test_ha_discovery_json PASS; evaluate.py --quick 0 FAIL. Not verified: on a device (AC3, bench); the FS-OTA KEEP_LIST path with a pending edit (the copy is flushed only when dirty; the unit reboots after an FS update).

Bench round 1 (09, .61, simulated Dallas, 2.0.0-beta.233+962a3c7): steps 1-5 PASS. Step 4 proves rename over an existing file on our precompiled esp_littlefs: after /ReBoot only dallas_labels.ini (103 B) is left, no .tmp. FINDING: GET /api/v2/sensors/labels answered 503 'Labels busy, retry' on ~30-40 % of reads for up to ~0.9 s after a POST. Cause: flushDallasLabelsLocked() held s_dlMutex across the whole flash write (open/write/close/rename), while GET/POST wait only 100 ms. Without a client retry, 3 of 10 back-to-back rename pairs lost the second rename. fetchWithRetry hides it in the web UI, not for API/HA clients.

Round 2 fix. flushDallasLabels() takes the lock only to copy the labels into a static s_dlFlushBuf (2049 B) and to read s_dlGen, a generation counter that dallasLabelsReplaceLocked() bumps. It writes temp + rename without the lock, then retakes it briefly: a failure sets the 30 s retry; a success clears dirty only when the generation is unchanged. A POST during the write therefore stays dirty, and the next pass writes it. Static, not malloc: deterministic, no failure path, no 2 KB heap churn; RAM +2 KB. One writer: every flush caller runs on the loop task (loopDallasLabels; doRestart from the nightly restart, the loop-deferred reboot and the WiFi supervisor), so s_dlFlushBuf needs no second lock.

Kept on purpose (coordinator agrees): ensureSensorDefaultLabels() still holds the lock across flush + file edit + reload. It runs only from initSensors(), at boot after startWebserver() and on a telnet rescan. Releasing the lock there would let the reload overwrite a POST made during the edit, or the POST drop the new defaults. A 503 in that boot window is retried by the web UI (fetchWithRetry).

Round 2 host test (tests/test_dallas_labels_ram_copy.py, new case test_post_during_the_flash_write_is_not_blocked_and_not_lost): the fake LittleFS runs a POST from inside File::write, and the lock stub refuses a timed lock while the lock is held, as a 100 ms wait against a flash write does. Old 962a3c7 (OTGW_SENSORS_INO=git show 962a3c7:...): FAIL '503' != '200'. Fix: 11/11 OK. Mutant without the generation check: FAIL, second POST lost ('{"28AA":"E"}' != '{"28AA":"F"}'). Next: after ba (TASK-1913) and d3 (TASK-1834) push, rebase, switch the temp + rename to ba's fsCommitTempFile() (helperStuff.ino), re-bump, build. 09 then re-runs steps 3 and 6 and compares the 503 count with the 233 baseline.

Bench 30-min load on 962a3c7 (09, evidence b233): 892 GET ok, 8 GET 503 (all within 3 s after a save, ~27 % of 30 saves), 0 POST 503, 0 wrong read-backs, no reset, heap 58 KB. Baseline for round 2: expect 0 GET 503. Second finding: deleting or uploading /dallas_labels.ini via FSexplorer left the RAM copy untouched, so GET served the old names and the next save wrote them back over the user's file.

Round 2, FSexplorer reload: dallasLabelsFileChanged(path) is called from all three places that change a file on async_tcp: the apilistfiles delete after LittleFS.remove, the handleFile ?delete, and handleFileUpload before the final close (File::path(), mount-relative, checked in framework vfs_api.cpp:398). Only the labels file counts, with or without the leading slash. It records the generation and sets a request flag (atomic, no lock); loopDallasLabels() consumes it before the flush. reloadDallasLabels() reads the file into s_dlFlushBuf outside the lock, then takes it over only if no POST came after the request: the last action wins, and an older pending edit is dropped, not written over the upload. Remaining window: a flush already writing when the upload lands can rename over it; that needs a save and an upload within the same ~1 s.

Round 2, shared helper: the flush now writes the temp file and commits it with ba's fsCommitTempFile() (TASK-1913, helperStuff.ino: size check, rename, temp removed on failure); the host test compiles that real block. Host tests on the final code (rebased on 5cc1612c0): tests/test_dallas_labels_ram_copy.py 12/12 OK. Old 962a3c7 sensors_ext.ino with a no-op dallasLabelsFileChanged shim: 2 FAIL, '503' != '200' (POST during the flash write) and '{"28AA":"P"}' != '{"28AA":"U"}' (stale copy after an upload). Mutant without the generation check: FAIL, second POST lost. Mutant without the reload: FAIL P != U. Old FSexplorer.ino: wiring FAIL. tests/fixtures/web_file_scheduling.cpp got a dallasLabelsFileChanged stub (it compiles handleFile()). Also OK: 1899 gate, flags_loop, health, dallas_first_reading, fs_update_keep, web_file_scheduling, fsexplorer_relative_urls, ota_flash, reboot_listener_and_reason, favicon_flash, test/host webserver_listener_retry and settings_atomic_write; evaluate --quick Failed 0, health 100 %.
<!-- SECTION:NOTES:END -->
