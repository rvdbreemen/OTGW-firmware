---
id: TASK-1960
title: >-
  Diag recording: POST /diag/recording/stop makes the PSRAM ring unreadable
  before a download
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-15T07:56:00Z'
updated_date: '2026-11-16T09:21:00Z'
labels:
  - diag
  - bench-finding
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 1003000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Bench 2026-11-15 08:32, .63 (Classic + S3, PSRAM), beta.250+aed38bf (found by otgw-bench-tester 09). A TASK-1666 recording had run for 10.5 h. Status before the stop: state recording, ring_blocks 384, newest_seq 127, dropped 0, exact true. POST /api/v2/diag/recording/stop answered result stopped, with ring_blocks 0 and has_data false. The GET /api/v2/diag/recording that followed returned 204 B (header only, Content-Length 204), so the recording was lost. Cause (read in the code at origin/dev 7e0cc5047): diagRecorderStuff.h stop() (line ~382) keeps the PSRAM block (memIsPsram; ADR-213 decision 3 says it is never freed), but stop() also sets ring = nullptr, haveBlock = false and used = 0. That makes hasData() (line ~398) false, so the download has nothing to stream, and the next start() overwrites the block. ADR-213 does not say whether the data stays readable after a stop. TASK-1700 warns 'Stop clears the recording, download it first' only on no-PSRAM boards; on PSRAM boards neither the UI nor the API warns. Evidence: 09 scratchpad n1666/end (status-pre.json, stop.json, status-post.json, recording.bin 204 B, dl-headers.txt). Related: TASK-1666, TASK-1700, ADR-213.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 On a PSRAM board, GET /api/v2/diag/recording after POST .../stop still returns the full recording (blocks oldest_seq..newest_seq) until a new start or an explicit clear; status shows state stopped and has_data true
- [ ] #2 Or, if keeping the data is rejected: the stop endpoint and the web UI Stop button warn on PSRAM boards too that the recording is discarded (extend the TASK-1700 warning)
- [x] #3 Host test on diagRecorderStuff.h: stop, then read the blocks; red on the current code, green on the fix
- [x] #4 Bench on .63: record, stop, download; the file size matches (newest_seq+1) x 4096 plus the header
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Route AC1 (keep data; AC2 warning route not needed). The UI already has 'Stopped, a recording is in memory, ready to download' and its webui test assumes has_data true after stop, so keeping the data is the designed intent; ADR-213 decision 3 keeps the PSRAM block, it is silent on readability. Change in diagRecorderStuff.h: stop() on PSRAM only clears active (ring/haveBlock/used/curSeq stay); heap path unchanged (ring freed). Writer::open() checks active under the lock (was ring==nullptr) so a capture point racing stop() drops. start() already reuses the PSRAM block and resets state, so the next Start begins a new recording. Tests: new F16 in test/host/test_diag_recorder.cpp (red old, green fix), R3/R7 in test_diag_recording_routes.cpp updated to the new contract. Bench AC4 by 09 on .63. Not for 1.x (no recorder there).
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
08:05 session 95, worktree wt-95-1960 (lane-95-1960 off origin/dev bd7a133c2). RED on old code: python test/host/test_diag_recorder.py -> F1-F15 pass, 'CASE F16 FAIL -- newest=7 hasData false after stop block 0 gone after the racing stop race wrote 0', RESULT FAIL (scratchpad t1960-red.txt). GREEN on fix: --mutant run: REAL passes every case (F1-F16), both mutants red as designed, RESULT PASS. test_diag_recording_routes.py: R1-R8 + W1-W4 pass after R3/R7 were changed from 'stopped = header-only' to 'PSRAM stopped serves keptBlocks, heap stopped header-only'. Also green: tests/test_diag_decode.py 10 OK, test_local_frame_consumers PASS, test_boiler_unsupported_origin PASS, webui diag-recording-controls 3/3, test_sat_loop_integration 15 OK, evaluate.py --quick 87 passed 0 failed.

09:4x (95) review follow-ups folded in (directive 08:35; TASK-1996 archived): (a) a Start during a running download: DiagRecorder::gen +1 per start(), DiagDownload::gen fixed by diagDownloadRange(); diagDownloadFill() serves a lost block when the generation changed (DIAG-GUARD-GEN). New R9 in test_diag_recording_routes.cpp (download of a stopped PSRAM ring, Start at the middle of block 1): fix pass; the GEN mutant fails exactly R9 ('block 2 served from the new recording'). (b) a heap stop() that loses lockWait: readable() = ring && haveBlock && (memIsPsram || active), so hasData()/block() serve nothing from a stopped heap ring; start() detaches and frees a leftover heap ring only under the lock (refuses when the lock stays busy). New F17 in test_diag_recorder.cpp: pass on the fix, FAIL on the f224a0b header ('after the lost stop: hasData/block still served'). Full harness runs: test_diag_recorder --mutant RESULT PASS (F1-F17, 3 mutants red as designed), test_diag_recording_routes --mutant RESULT PASS (R1-R9 + W1-W4, 3 mutants red as designed). Restacked on top of TASK-1990 (9c6851dd1) per the coordinator: one lane, 1990 first.

Bench 09 on .63 (Classic, PSRAM), 2.0.0-beta.344+873f06b, 10:00-10:16 (09 scratchpad b344\): OTA app+fs OK, bootcount 160->162 steady, settings byte-identical. Start: psram true, ring_blocks 384. After 527 s newest_seq 2; POST stop -> state stopped, has_data true, newest_seq 2, dropped 0. Download after stop = 12492 B = 204 + (2+1) x 4096, 0 lost, blocks 0/1 byte-identical to a pre-stop download; diag_decode: 3 blocks, 1099 records over 527.7 s, 0 missing/lost/dropped. Second start: has_data true at once because start writes its own setting record (DiagRecordingOn) first; the AC1 contract (readable until a new start) holds; the next download is the new recording (4300 B, 156 records). Start-during-download not exercised on the bench (12 KB download ended first); covered by host R9 + DIAG-GUARD-GEN mutant. Pushed ca906b210..8d90c4c8a as 3c8dc8137 (beta.344, src diff to the benched 873f06b9b = 0). No own post: ba's next build covers 343/344 (coordinator). Bench ACs (AC1 board part, AC4) for 09 to tick.

2026-11-16 10:04-10:16 bench 09 on .63 (Classic S3, PSRAM 2 MB), 2.0.0-beta.344+873f06b (src identical to pushed 3c8dc8137). Start: psram true, ring_blocks 384, ring_bytes 1572864. After 527 s newest_seq 2; download before the stop 204 + 3 x 4096 = 12492 B. POST stop -> state stopped, has_data true, newest_seq 2, oldest 0, unchanged 5 s later. GET /api/v2/diag/recording after the stop: 12492 B = header 204 + (newest_seq+1) x 4096, first 0, count 3, 0 lost; header and blocks 0-1 byte-identical to the pre-stop download (block 2 was the open block and gained records before the stop). scripts/diag_decode.py: 3 blocks, missing 0, lost 0, 1099 records over 527.7 s, dropped 0 (ble 68, mqtt_pub 485, ot 528, pic_cmd 7, response 7, rest_in 1, setting 2, weather 1). A new start makes a new recording: has_data true at once because the start writes its own setting record (DiagRecordingOn), newest_seq 0; a download 70 s later is 204 + 1 x 4096 = 4300 B, block 0 differs from the old block 0, decode 156 records, 0 missing/lost/dropped. Start during a download of the stopped ring not exercised on the bench (the 12 KB download finished first); covered by host test R9 and its mutant (95). Settings byte-identical, bootcount only +2 for the OTA. Evidence: 09 scratchpad b344/diag/ (diag.log, rec1_before_stop.bin, rec1_after_stop.bin, rec2.bin, decode.txt, diag1960.py).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
AC1: on a PSRAM board the recording stays readable after stop until the next start: host F16 (red on the old code 'hasData false after stop', green on the fix) and 09's bench on .63 (stop -> has_data true; download 204 + 3 x 4096 B, 0 lost, decode 1099 records). AC2: not needed, AC1 route taken (the UI already says 'Stopped, a recording is in memory, ready to download'). AC3: host tests on the real diagRecorderStuff.h: F16/F17 (test_diag_recorder, 3 mutants red as designed), R3/R7 updated and R9 new (test_diag_recording_routes, the GEN mutant fails exactly R9). AC4: 09's bench above. Review follow-ups finished here (TASK-1996 archived): Start during a running download serves lost blocks (R9), a heap stop that loses the lock leaves no readable ring and start() frees it under the lock (F17). Not verified on hardware: the Start-during-download path (host only).
<!-- SECTION:FINAL_SUMMARY:END -->
