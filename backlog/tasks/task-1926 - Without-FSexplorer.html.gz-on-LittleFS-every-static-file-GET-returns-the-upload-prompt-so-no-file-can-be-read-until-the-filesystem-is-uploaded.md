---
id: TASK-1926
title: >-
  Without FSexplorer.html.gz on LittleFS every static file GET returns the
  upload prompt, so no file can be read until the filesystem is uploaded
status: Done
assignee:
  - '@claude'
created_date: '2026-11-14T08:18:00Z'
updated_date: '2026-11-14T13:45:00Z'
labels:
  - bug
  - webui
  - filesystem
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 987000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Seen by otgw-bench-tester 09 in TASK-1774 round 2 (.63, 2026-11-14 ~09:07, evidence 09 scratchpad r1774b\ev\): after the boot-time format of a half-written image the filesystem holds only settings.ini, reboot_count.txt, reboot_log.txt, bootdetect.log and the TASK-1495 marker. Every static GET (also /.fs_update_incomplete and /settings.ini) answered the built-in Helper page (466 B) until FSexplorer.html.gz was uploaded by hand. Code: FSexplorer.ino:572, the file-read handler sends Helper whenever neither /FSexplorer.html.gz nor /FSexplorer.html exists, before it looks at the requested path. /update (from firmware flash) and the REST API keep working, so recovery by a filesystem upload works; what is lost is reading or downloading files (the marker, the reboot log, a settings backup) in that state. TASK-1774 makes this state more frequent (a reset during a filesystem write now formats and restores instead of leaving a broken image). Direction, not decided: send Helper only for the page routes (/, /FSexplorer.html) and serve existing files as usual; keep the ADR-056 auth guard on settings.ini.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Host or bench reproduction: with FSexplorer.html.gz absent, GET /reboot_log.txt returns the upload prompt instead of the file (old)
- [x] #2 Fix: existing files are served (auth guard unchanged), the prompt stays for the page routes; old red, fix green
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Move the upload-prompt gate in handleFile() (FSexplorer.ino) from before the file lookup to the not-found branch: an existing file is served (ADR-056 settings.ini/settings.tmp auth guard unchanged and still first), and only a missing path without any recovery page (no /FSexplorer.html.gz and no /FSexplorer.html) gets the Helper prompt. Page routes keep their own Helper fallback (sendFSexplorerHtml, sendIndex). Test: tests/test_web_file_scheduling.py compiles the real handleFile(); its no-recovery-page case changes from 'prompt for settings.ini' to: /reboot_log.txt, /.fs_update_incomplete, /settings.ini served 200; settings.ini with a failed auth check not served; a missing path gets the prompt. checkHttpAuth() in the fixture becomes switchable. Red on origin/dev (--old-rev), green on the fix, all 5 mutants still killed. Bench on .63: FS without FSexplorer.html.gz (remove it via FSexplorer delete), GET /reboot_log.txt returns the file, GET of a missing path returns the prompt, then restore with a clean FS OTA. Not on otgw-1.x.x (2.x FSexplorer code).
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-14 (09) host evidence. tests/test_web_file_scheduling.py with the updated no-recovery-page case: on the current code (and --old-rev origin/dev) FAIL 4: lines 603 x3 (reboot_log.txt, .fs_update_incomplete, settings.ini answered the prompt, path empty) and 608 (settings.ini with a failed password check got the 200 prompt, because the prompt ran before the ADR-056 guard). On the fix: PASS 0 failures. Mutants still killed (exit 0 each, e.g. handlefile-gz-only-gate: 'KILLED (fixture failed as it must)' at line 615): handlefile-gz-only-gate, no-plain-fallback, double-encoding-file, double-encoding-progmem, etag-without-path. Related tests OK: test_fsexplorer_relative_urls, test_health_no_fs_flash_ops, test_dallas_labels_ram_copy. evaluate.py --quick: 92 checks, 0 failed (same FAIL set as the main tree).

2026-11-14 (09) bench .63 on 2.0.0-beta.245+662a2a8 (pushed 662a2a859; 1926 is d7a40ea72): after deleting FSexplorer.html.gz, GET /reboot_log.txt 200 1642 B with the log, /reboot_count.txt 200 '114', /version.hash 200 '662a2a8'; a missing path (/nothing.ini) 200 with the upload prompt (466 B); directories /sat, /i18n, /sat/ 200 with 0 B (no crash). A clean FS OTA restored FSexplorer; settings diff [] (API and raw ini). Evidence: 09 scratchpad b245/.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
AC1: host test red on the old handleFile() (existing files answered the upload prompt without a recovery page). AC2: fix moves the prompt to the not-found branch; host test green, mutants killed; bench .63 beta.245: files served without FSexplorer, prompt only for a missing path, settings unchanged. Not verified: none open.
<!-- SECTION:FINAL_SUMMARY:END -->
