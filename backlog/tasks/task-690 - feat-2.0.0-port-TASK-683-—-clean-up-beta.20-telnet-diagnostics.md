---
id: TASK-690
title: 'feat-2.0.0: port TASK-683 — clean up beta.20 telnet diagnostics'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-05-24 08:18'
updated_date: '2026-05-24 08:23'
labels:
  - port-from-dev
  - diagnostics
  - log-cleanup
dependencies: []
priority: medium
ordinal: 58000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Port of dev TASK-683 / commit 554e7eda. Cleans up four diagnostic surfaces in the firmware so future field logs are easier to read: FSexplorer onNotFound emits accurate outcome lines, apifirmwarefilelist drops JSON mirroring/per-iteration spam, GetVersion drops its entry trace, checklittlefshash is silent on match. No behaviour changes to file serving, version detection or git-hash checking.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 FSexplorer.ino onNotFound emits one accurate line per static request: 'http GET /path => 200 (file)' (gated on state.debug.bRestAPI) or 'http GET /path => 404' (always-on). No 'onNotFound' wording remains.
- [x] #2 apifirmwarefilelist no longer mirrors JSON output to telnet (no bare '[', ',', ']' lines, no per-entry JSON dump, no '--- banner ---' lines). Function entry + per-file GetVersion result lines are gated on state.debug.bRestAPI. One always-on summary 'api firmware/files: N entries (Xms)' is emitted at end.
- [x] #3 versionStuff.ino GetVersion no longer emits the 'GetVersion opening <path>' entry trace. The 'banner not found in <path>' warning is preserved.
- [x] #4 checklittlefshash is silent on match. The mismatch path still emits the existing WARNING block and StatusMessage update.
- [x] #5 python build.py --firmware exits 0.
- [x] #6 python evaluate.py --quick shows no new failures vs current 2.0.0 baseline.
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. FSexplorer.ino onNotFound: replace 'next: handleFile/in onNotFound' lines with one outcome-bearing line ('http GET => 200 (file)' gated bRestAPI, or 'http GET => 404' always-on); early-return after processAPI.
2. FSexplorer.ino apifirmwarefilelist: drop 'banner', '[', ',', ']' mirroring; drop per-entry telnet mirror and dir.fileName line; gate function-entry and per-file GetVersion result lines on bRestAPI; track elapsed ms + count; add always-on summary 'api firmware/files: N entries (Xms)'.
3. versionStuff.ino: drop unconditional 'GetVersion opening %s' entry trace.
4. helperStuff.ino: gate both DebugTf calls inside !match branch so happy path is silent.
5. Bump prerelease (alpha.57 -> alpha.58).
6. Build + evaluator.
7. Commit.
<!-- SECTION:PLAN:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Ported dev TASK-683 to 2.0.0: cleaned up four diagnostic surfaces so future field logs are easier to read.

## Changes
- FSexplorer.ino onNotFound: replaced 'in onNotFound!! / next: handleFile' chatty trio with one outcome-bearing line per request — 'http GET /path => 200 (file)' (bRestAPI-gated) or 'http GET /path => 404' (always-on). Early-return after processAPI() to flatten the branches.
- FSexplorer.ino apifirmwarefilelist: dropped JSON-mirror-to-telnet noise (banner, bare brackets/commas, per-entry mirror, per-iteration entry name). Function entry + dirpath + per-file GetVersion result + verfile write trace now gated on bRestAPI. Added always-on summary 'api firmware/files: N entries (Xms)' at end (handles the no-dir early-return path too).
- versionStuff.ino GetVersion: dropped unconditional 'GetVersion opening %s' entry trace; banner-not-found warning preserved.
- helperStuff.ino checklittlefshash: silent on hash match; mismatch path retains the FS/FW pair log line + WARNING block + statusMessage update.

## Notes vs dev
- 2.0.0 uses PlatformDir + entryName/entrySize instead of dev's LittleFS.openDir + dir.fileName().c_str(); the patch follows 2.0.0 conventions.
- No-dir early-return path (specific to 2.0.0) also emits the summary line so the summary fires unconditionally.

## Verification
- python build.py --firmware --target esp8266 exits 0 (2.0.0-alpha.58+bd576af).
- python evaluate.py --quick: 60/1/0 (98.5%; the one warning is the pre-existing mqtt_configuratie.cpp filename-difference info, unchanged from baseline).
<!-- SECTION:FINAL_SUMMARY:END -->
