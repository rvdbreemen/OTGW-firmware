---
id: TASK-1996
title: >-
  Diag recording: a Start during a running download can mix two recordings; a
  heap stop that loses the lock leaves the ring allocated
status: To Do
assignee: []
created_date: '2026-11-16T07:12:00Z'
updated_date: '2026-11-16T08:31:00Z'
labels:
  - diag
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 1021000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found by the opus review of TASK-1960 (2026-11-16, read in code, not run). (1) diagDownloadFill() checks only the block_seq range (DiagRecorder::block()), not which recording a block belongs to. Since TASK-1960 a stopped PSRAM ring stays downloadable, and the UI keeps Start enabled while the browser download streams (diagRecBusy covers fetch calls only, v2.js ~6100). A Start mid-download resets curSeq: when the old range began at block 0 the filler serves blocks of the new recording under the old numbers, or a torn block (old chunks then new). Fix idea: a recording generation counter bumped by start(), stored in DiagDownload at diagDownloadRange(); the filler serves a lost block when it changed. (2) Pre-existing: a heap-ring stop() that loses lockWait(50) leaves mem/ring allocated with has_data true (against ADR-213 and the UI 'Stop clears' warning); the next start() then frees mem at the 'if (mem && !memIsPsram)' line without the lock while an async_tcp filler may copy from it (possible use-after-free, window not verified on a device).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 A host test starts a new recording while a download of the stopped ring is half served: the rest is served as lost blocks, never as blocks of the new recording; red on the code after TASK-1960, green on the fix
- [ ] #2 A heap stop() that loses the lock does not leave a readable ring, and start() never frees the ring while a filler can read it; host test red then green
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Folded into TASK-1960 (maintainer directive 2026-11-16 08:35 via the coordinator: a fix found while working a task is finished under that task). Both items fixed and tested there (R9 + DIAG-GUARD-GEN mutant, F17). Archived.
<!-- SECTION:NOTES:END -->
