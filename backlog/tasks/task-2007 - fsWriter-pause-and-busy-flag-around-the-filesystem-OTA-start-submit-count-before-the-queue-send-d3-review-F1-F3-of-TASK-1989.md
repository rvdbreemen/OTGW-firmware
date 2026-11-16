---
id: TASK-2007
title: >-
  fsWriter: pause and busy flag around the filesystem-OTA start; submit count
  before the queue send (d3 review F1-F3 of TASK-1989)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-16T19:46:00Z'
updated_date: '2026-11-16T21:38:00Z'
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
- [x] #1 Host test on the real fsWriter: a submit during an OTA-start drain is not lost and never runs after LittleFS.end(); red on the current code, green on the fix
- [x] #2 Host test: fsWriterPending() never reads 0 while a submitted job is not yet written (count before the send)
- [x] #3 A job over 256 B is refused with a telnet line, not cut
- [ ] #4 Bench by the bench-tester: fs OTA while SAT saves run (where a SAT tick is available), sat_*.json intact
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-16 (95, taken over with ba's OK; ba: nothing local). Fix in lane-95-2007 (worktree wt-95-2007, not pushed; build + bench tomorrow after d3). fsWriter.ino: F1 the writer task runs fsWriterStep() in a loop (queue read without waiting, 20 ms delay when idle or held); fsWriterStep() sets s_fswBusy, then checks s_fswPaused / bFsImageWriting / !LittleFSmounted, takes one job, writes it, clears busy; fsWriterPauseAndFlush(timeout) sets paused, waits until !busy (false on timeout, queue left as it is), then writes every queued job on the caller; fsWriterResume() clears paused. The flags and both counters are std::atomic (seq_cst): the writer stores busy then loads paused, the pauser stores paused then loads busy, so one always sees the other. OTGW-ModUpdateServer-esp32.h FS-OTA start: fsWriterPauseAndFlush(2000) (logs '[OTA] fswriter: still busy after 2 s' on timeout) -> _keepFiles() -> fsOtaRtcFill() -> LittleFS.end() -> bFsImageWriting = true -> fsWriterResume(); a save queued after the flush waits for the remount and lands after the restored copies. F2 s_fswSubmitted++ before platformQueueSend, -- when it fails (also around the queue-full drain, so that drain does not wait for this job). F3 a job over FSW_DATA_LEN (256 B) or a path of FSW_PATH_LEN (32) chars or more is refused with '[fswriter] <path> refused: <n> B data, ...', never cut (the path was also cut silently by strlcpy before). Heap: FSW_QUEUE_DEPTH 6 -> 3 (one slot per state file, ~870 B back); the heap telnet line (logHeapStats) gets fsw_stack_free=<bytes> via platformIpcStackHighWater("fswriter") (works for any task name); the 4096 B stack stays until field values exist.

Tests (tests/test_fs_writer.py, the real fsWriter.ino; OTGW_FSW_INO/OTGW_FSW_H point at another copy; the writer pass is now the real fsWriterStep() instead of a re-implemented task body): FIX 16/16 OK. OLD (origin fsWriter.ino/.h) 4 FAIL: F1(a) 'final {old}' (the save after the copy is lost at the restore), F1(b) 'after_end_writes 1' (the writer wrote in the gap between LittleFS.end() and bFsImageWriting = true), F2 'pending_during_send 0', F3 'submit 1' with the 300 B cut to 256 B; 2 skipped on old (the flush wait and timeout cases need the new API). New cases also cover the flush waiting for a busy writer and the 2 s timeout (returns false, queue kept). Known limit, kept on purpose: after that timeout the OTA logs and goes on, so a writer stuck over 2 s inside one job could still be in open/write at LittleFS.end(). Related tests via run_tests.py (15 files that read helperStuff.ino / the OTA header / fsWriter) all PASS; test_sat_pid_persist, test_sat_pid_cadence, test_sat_restart_quiet, test_sat_sim_no_learner_save, test_fs_update_keep OK; evaluate.py --quick 89/0. Not verified yet: the seq_cst handshake and <atomic> on the target (build tomorrow), AC4 bench.
<!-- SECTION:NOTES:END -->
