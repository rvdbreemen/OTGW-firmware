---
id: TASK-1960
title: >-
  Diag recording: POST /diag/recording/stop makes the PSRAM ring unreadable
  before a download
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-15T07:56:00Z'
updated_date: '2026-11-16T08:31:00Z'
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
- [ ] #1 On a PSRAM board, GET /api/v2/diag/recording after POST .../stop still returns the full recording (blocks oldest_seq..newest_seq) until a new start or an explicit clear; status shows state stopped and has_data true
- [ ] #2 Or, if keeping the data is rejected: the stop endpoint and the web UI Stop button warn on PSRAM boards too that the recording is discarded (extend the TASK-1700 warning)
- [x] #3 Host test on diagRecorderStuff.h: stop, then read the blocks; red on the current code, green on the fix
- [ ] #4 Bench on .63: record, stop, download; the file size matches (newest_seq+1) x 4096 plus the header
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Route AC1 (keep data; AC2 warning route not needed). The UI already has 'Stopped, a recording is in memory, ready to download' and its webui test assumes has_data true after stop, so keeping the data is the designed intent; ADR-213 decision 3 keeps the PSRAM block, it is silent on readability. Change in diagRecorderStuff.h: stop() on PSRAM only clears active (ring/haveBlock/used/curSeq stay); heap path unchanged (ring freed). Writer::open() checks active under the lock (was ring==nullptr) so a capture point racing stop() drops. start() already reuses the PSRAM block and resets state, so the next Start begins a new recording. Tests: new F16 in test/host/test_diag_recorder.cpp (red old, green fix), R3/R7 in test_diag_recording_routes.cpp updated to the new contract. Bench AC4 by 09 on .63. Not for 1.x (no recorder there).
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
08:05 session 95, worktree wt-95-1960 (lane-95-1960 off origin/dev bd7a133c2). RED on old code: python test/host/test_diag_recorder.py -> F1-F15 pass, 'CASE F16 FAIL -- newest=7 hasData false after stop block 0 gone after the racing stop race wrote 0', RESULT FAIL (scratchpad t1960-red.txt). GREEN on fix: --mutant run: REAL passes every case (F1-F16), both mutants red as designed, RESULT PASS. test_diag_recording_routes.py: R1-R8 + W1-W4 pass after R3/R7 were changed from 'stopped = header-only' to 'PSRAM stopped serves keptBlocks, heap stopped header-only'. Also green: tests/test_diag_decode.py 10 OK, test_local_frame_consumers PASS, test_boiler_unsupported_origin PASS, webui diag-recording-controls 3/3, test_sat_loop_integration 15 OK, evaluate.py --quick 87 passed 0 failed.

09:4x (95) review follow-ups folded in (directive 08:35; TASK-1996 archived): (a) a Start during a running download: DiagRecorder::gen +1 per start(), DiagDownload::gen fixed by diagDownloadRange(); diagDownloadFill() serves a lost block when the generation changed (DIAG-GUARD-GEN). New R9 in test_diag_recording_routes.cpp (download of a stopped PSRAM ring, Start at the middle of block 1): fix pass; the GEN mutant fails exactly R9 ('block 2 served from the new recording'). (b) a heap stop() that loses lockWait: readable() = ring && haveBlock && (memIsPsram || active), so hasData()/block() serve nothing from a stopped heap ring; start() detaches and frees a leftover heap ring only under the lock (refuses when the lock stays busy). New F17 in test_diag_recorder.cpp: pass on the fix, FAIL on the f224a0b header ('after the lost stop: hasData/block still served'). Full harness runs: test_diag_recorder --mutant RESULT PASS (F1-F17, 3 mutants red as designed), test_diag_recording_routes --mutant RESULT PASS (R1-R9 + W1-W4, 3 mutants red as designed). Restacked on top of TASK-1990 (9c6851dd1) per the coordinator: one lane, 1990 first.
<!-- SECTION:NOTES:END -->
