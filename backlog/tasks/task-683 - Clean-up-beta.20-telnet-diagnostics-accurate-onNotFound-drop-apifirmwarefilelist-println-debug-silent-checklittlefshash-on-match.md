---
id: TASK-683
title: >-
  Clean up beta.20 telnet diagnostics: accurate onNotFound + drop
  apifirmwarefilelist println-debug + silent checklittlefshash on match
status: In Progress
assignee:
  - '@claude'
created_date: '2026-05-24 06:20'
updated_date: '2026-05-24 06:20'
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
- [ ] #1 FSexplorer.ino onNotFound emits one accurate line per static request: 'http GET /path => 200 (file)' (gated on state.debug.bRestAPI) or 'http GET /path => 404' (always-on). No 'onNotFound' wording remains.
- [ ] #2 apifirmwarefilelist no longer mirrors JSON output to telnet (no bare '[', ',', ']' lines, no per-entry JSON dump, no '--- banner ---' lines). Function entry + per-file GetVersion result lines are gated on state.debug.bRestAPI. One always-on summary 'api firmware/files: N entries (Xms)' is emitted at end.
- [ ] #3 versionStuff.ino GetVersion no longer emits the 'GetVersion opening <path>' entry trace. The 'banner not found in <path>' warning is preserved.
- [ ] #4 checklittlefshash is silent on match. The mismatch path still emits the existing WARNING block. The boot banner 'fs:ok' / 'fs:mismatch' is unchanged.
- [ ] #5 python build.py --firmware exits 0.
- [ ] #6 python evaluate.py --quick shows no new failures vs current dev.
<!-- AC:END -->
