---
id: TASK-1960
title: >-
  Diag recording: POST /diag/recording/stop makes the PSRAM ring unreadable
  before a download
status: To Do
assignee: []
created_date: '2026-11-15T07:56:00Z'
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
- [ ] #3 Host test on diagRecorderStuff.h: stop, then read the blocks; red on the current code, green on the fix
- [ ] #4 Bench on .63: record, stop, download; the file size matches (newest_seq+1) x 4096 plus the header
<!-- AC:END -->
