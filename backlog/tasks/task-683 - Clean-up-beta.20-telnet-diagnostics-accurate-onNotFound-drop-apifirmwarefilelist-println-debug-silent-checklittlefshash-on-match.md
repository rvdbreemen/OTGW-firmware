---
id: TASK-683
title: >-
  Clean up beta.20 telnet diagnostics: accurate onNotFound + drop
  apifirmwarefilelist println-debug + silent checklittlefshash on match
status: In Progress
assignee:
  - '@claude'
created_date: '2026-05-24 06:20'
updated_date: '2026-05-24 06:26'
labels:
  - diagnostics
  - beta.20
  - log-cleanup
dependencies: []
references:
  - OTGW_1.6_beta_20.txt (crashevans
  - FW 1.6.0-beta.20+591131f
  - 'build #3380)'
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
From the beta.20 telnet log (crashevans, UK, FW 1.6.0-beta.20+591131f) the firmware emits several diagnostic lines that are either misleading, redundant, or pure println-debug. This task makes the existing diagnostics accurate and quiets the always-on noise so future field logs are easier to read.

Targets (with line refs from OTGW_1.6_beta_20.txt):
- FSexplorer.ino:275 onNotFound lambda — "onNotFound: handleFile(/path)" sounds like a 404 but fires for every static request whether the file exists or not; the real 404 path emits nothing on telnet.
- FSexplorer.ino:287-374 apifirmwarefilelist — 13+ always-on lines per Update-page open (function entry, dirpath, bare JSON brackets, per-entry mirror, banner lines), all ungated.
- versionStuff.ino:19 GetVersion — "GetVersion opening <path>" entry trace, ungated; called 3x per Update-page open.
- helperStuff.ino:745-746 checklittlefshash — two duplicate lines fired every minute even when FS/FW hashes match; only the mismatch case is actionable.

Out of scope: any behaviour change in file serving, version detection, or git-hash checking. Performance work on apifirmwarefilelist is a separate task.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 FSexplorer.ino onNotFound emits one accurate line per static request: 'http GET /path => 200 (file)' (gated on state.debug.bRestAPI) or 'http GET /path => 404' (always-on). No 'onNotFound' wording remains.
- [x] #2 apifirmwarefilelist no longer mirrors JSON output to telnet (no bare '[', ',', ']' lines, no per-entry JSON dump, no '--- banner ---' lines). Function entry + per-file GetVersion result lines are gated on state.debug.bRestAPI. One always-on summary 'api firmware/files: N entries (Xms)' is emitted at end.
- [x] #3 versionStuff.ino GetVersion no longer emits the 'GetVersion opening <path>' entry trace. The 'banner not found in <path>' warning is preserved.
- [x] #4 checklittlefshash is silent on match. The mismatch path still emits the existing WARNING block. The boot banner 'fs:ok' / 'fs:mismatch' is unchanged.
- [x] #5 python build.py --firmware exits 0.
- [x] #6 python evaluate.py --quick shows no new failures vs current dev.
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. FSexplorer.ino:267-282 onNotFound lambda
   - Decode URI once into a String, call handleFile with a copy (handleFile takes String&&), branch on result.
   - If !served: send 404 (unchanged behaviour).
   - Emit one line per request:
     * served: DebugTf(PSTR("http GET %s => 200 (file)\r\n"), path.c_str()) gated on state.debug.bRestAPI
     * 404: DebugTf(PSTR("http GET %s => 404\r\n"), path.c_str()) always-on
   - Early-return after processAPI() to flatten the branches.

2. FSexplorer.ino:287-374 apifirmwarefilelist
   - Gate the existing API/dirpath function-entry lines on state.debug.bRestAPI.
   - Drop all DebugTln/DebugTf calls that mirror JSON output to telnet (lines 309, 310, 350, 362, 373, 374) and the per-iteration dir.fileName line (314).
   - Gate the per-file GetVersion(...) returned [...] line (335) on state.debug.bRestAPI.
   - Drop the bare Debugln() at 345.
   - Track elapsed millis and emit one always-on summary at end: DebugTf(PSTR("api firmware/files: %u entries (%lums)\r\n"), entryCount, elapsed).

3. versionStuff.ino:19 GetVersion
   - Drop the unconditional DebugTf(PSTR("GetVersion opening %s\r\n"),hexfile) line.
   - Keep the "banner not found in %s" warning at line 117.

4. helperStuff.ino:745-746 checklittlefshash
   - Move the two DebugTf calls inside an "if (!match)" branch so the happy path is silent.
   - Keep the existing WARNING block + state.statusMessage update on mismatch.

5. python build.py --firmware  → expect exit 0.
6. python evaluate.py --quick   → expect no new failures vs dev.
7. git add changed files, commit with descriptive message referencing TASK-683.
8. git push -u origin claude/beta-20-log-review-7gnaR.
9. Open draft PR.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Implemented + verified locally:
- FSexplorer.ino:267-282 onNotFound rewrite with served/404 branching.
- FSexplorer.ino:287-374 apifirmwarefilelist JSON mirroring dropped, traces gated, summary added.
- versionStuff.ino:19 GetVersion entry trace removed.
- helperStuff.ino:730-758 checklittlefshash silent on match.

Build: python build.py --firmware -> exit 0 (1.6.0-beta.20+607861e).
Evaluator: python evaluate.py --quick -> 34 passed / 0 warnings / 0 failures (100%).
Commit: 554e7eda.
Push: claude/beta-20-log-review-7gnaR.
Draft PR: https://github.com/rvdbreemen/OTGW-firmware/pull/640
<!-- SECTION:NOTES:END -->
