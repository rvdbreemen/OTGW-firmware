---
id: TASK-1926
title: >-
  Without FSexplorer.html.gz on LittleFS every static file GET returns the
  upload prompt, so no file can be read until the filesystem is uploaded
status: To Do
assignee: []
created_date: '2026-11-14T08:18:00Z'
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
- [ ] #1 Host or bench reproduction: with FSexplorer.html.gz absent, GET /reboot_log.txt returns the upload prompt instead of the file (old)
- [ ] #2 Fix: existing files are served (auth guard unchanged), the prompt stays for the page routes; old red, fix green
<!-- AC:END -->
