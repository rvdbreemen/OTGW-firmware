---
id: TASK-2007
title: >-
  fsWriter: pause and busy flag around the filesystem-OTA start; submit count
  before the queue send (d3 review F1-F3 of TASK-1989)
status: To Do
assignee:
  - '@claude'
created_date: '2026-11-16T19:46:00Z'
labels:
  - fs
  - sat
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 1028000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Second-reader review by d3 of TASK-1989 (7e1ae2649, 2026-11-16, code reading). F1 (medium, pre-existing race, not a regression): the fs-OTA start in OTGW-ModUpdateServer-esp32.h runs fsWriterDrain(2000) -> _keepFiles() -> fsOtaRtcFill() -> LittleFS.end() -> state.flash.bFsImageWriting = true on the async task while the loop keeps submitting saves; a job submitted after the drain passes the writer guard (fsWriter.ino:59 waits only on bFsImageWriting || !LittleFSmounted, and LittleFSmounted stays true across end()). Effects: (a) a write newer than the _keepFiles heap copy is lost at the restore, (b) the writer can be inside open/write while end() unmounts. Before TASK-1989 the loop wrote without any fs guard, so the same race existed unguarded. Fix: a pause flag set before the drain (fsWriterSubmit then waits or refuses) and a busy flag around fsWriterService(); at the OTA start: pause, wait for !busy, run the queued jobs on the caller, then _keepFiles/end. F2 (low, new in 1989): s_fswSubmitted is incremented after platformQueueSend, so fsWriterPending() can read 0 between the send and the increment and a drain from another task returns early: count before the send, undo on failure. F3 (nit): a job over 256 B is cut silently: refuse and log.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Host test on the real fsWriter: a submit during an OTA-start drain is not lost and never runs after LittleFS.end(); red on the current code, green on the fix
- [ ] #2 Host test: fsWriterPending() never reads 0 while a submitted job is not yet written (count before the send)
- [ ] #3 A job over 256 B is refused with a telnet line, not cut
- [ ] #4 Bench by the bench-tester: fs OTA while SAT saves run (where a SAT tick is available), sat_*.json intact
<!-- AC:END -->
