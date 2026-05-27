---
id: TASK-730
title: 'fix(restAPI): stream SAT markers POST/DELETE to fix loopTask stack overflow'
status: In Progress
assignee:
  - claude
created_date: '2026-05-27 18:31'
updated_date: '2026-05-27 18:33'
labels:
  - bug
  - crash
  - esp32
  - restapi
dependencies: []
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
George (sergeantd) reported ESP32-S3 crashes with 'Stack canary watchpoint triggered (loopTask)'. Root cause: POST and DELETE handlers for /api/v2/sat/markers each allocate two 2048-byte buffers simultaneously on the loopTask stack (4KB total), exhausting the 8KB default FreeRTOS stack budget when combined with NetworkClient and deep call chains.\n\nGET already streams correctly. POST and DELETE must be refactored to use a streaming/temp-file pattern with ~256-byte chunk buffers instead.\n\nSecondary fix: the 2048-byte buffer is too small for 20 full markers (~2.4KB), causing silent truncation on a full marker set.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 POST /api/v2/sat/markers reads existing markers in streaming fashion (no >512-byte stack buffer)
- [ ] #2 POST handler appends new marker via temp file + LittleFS.rename(), not in-memory concat
- [ ] #3 DELETE /api/v2/sat/markers/<id> filters markers via one-pass stream to temp file + rename
- [ ] #4 Neither POST nor DELETE allocates more than 512 bytes on the stack for JSON buffers
- [ ] #5 python build.py exits 0 in the 2.0.0 worktree after the change
- [ ] #6 python evaluate.py --quick shows no new failures
- [ ] #7 A marker set of 20 entries can be POSTed and DELETEd without data loss
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Add readNextMarkerObj lambda (inline, per handler) that reads one {…} object from a File, using a depth counter for correctness. Max ~200-byte buf arg, no hidden allocation.\n2. Rewrite POST handler:\n   - Pass 1: stream with readNextMarkerObj to count markers and find maxId (objBuf[200] on stack)\n   - Pass 2: open /sat_markers.tmp for write, stream original file object-by-object, append newentry[192], rename tmp -> original\n   - Stack: ~450 bytes total (was 4096)\n3. Rewrite DELETE handler:\n   - One-pass filter: open source + tmp, stream object-by-object, skip matching delId, write rest, rename\n   - Stack: ~240 bytes total (was 4096)\n4. GET handler: unchanged (already streams correctly)\n5. Run python build.py in 2.0.0 worktree, verify exit 0\n6. Run python evaluate.py --quick, verify no new failures\n7. Commit on feature-dev-2.0.0-otgw32-esp32-sat-support, push to origin
<!-- SECTION:PLAN:END -->
