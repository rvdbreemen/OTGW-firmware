---
id: TASK-2924
title: >-
  Telnet debug help: the H) heap-hold line prints on 'l' (toggle MyDEBUG)
  instead of in the 'h' menu
status: Done
assignee:
  - '@claude'
created_date: '2026-11-19T18:36:00Z'
updated_date: '2026-11-19T19:14:00Z'
labels:
  - telnet
  - debug
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 1061000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found by otgw-docs, 2026-11-19: handleDebug.ino prints 'H) bench: starve the heap / release it (prerelease builds only)' inside case 'l' (toggle MyDEBUG), so the 'h' help menu never lists the H key and pressing l prints an unrelated line. Prerelease builds only (#if defined(_VERSION_PRERELEASE)).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 The H) line is printed in the 'h' help menu (prerelease builds only) and no longer by case 'l'
- [x] #2 Host check on the real handleDebug.ino source: red on the old placement, green on the fix
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-19 otgw-dev: H) help line moved from case 'l' to the 'h' menu (after z), inside the _VERSION_PRERELEASE guard. tests/test_debug_help_heaphold.py reads the real handleDebug.ino: red on the old placement (2 of 3 failing: H) not in the 'h' block, H) printed by 'l'), green 3/3 on the fix; tests/test_heaphold_hook.py 8/8. Bench for AC1 (09, .61): 'h' lists H), 'l' only toggles MyDEBUG.

2026-11-19 bench (otgw-bench-tester, .61): PASS on 2.0.0-beta.397+7e31bc2 (app-only OTA 20:01, app sha256 063d7de4e1a3b11f, settings changed none, bootcount 1078). Telnet 'h': line 26 '  H) bench: starve the heap / release it (prerelease builds only)' directly after line 25 '  z) Reset heap soak diagnostics ...'. Telnet 'l' twice: only 'MyDEBUG: true' then 'MyDEBUG: false', no H) line. Regression 20:03-20:13: 10/10 device/info 200, bootcount unchanged, 0 panic/abort/Guru, MQTT connected, reboot_log only the OTA line. Capture: 09's scratchpad b397\menu.raw and b397\reg. AC1 ticked by otgw-dev on 09's evidence, at 09's request (the task file was not yet on origin).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
The prerelease telnet help line 'H) bench: starve the heap / release it' moved from case 'l' (toggle MyDEBUG) into the 'h' help menu, after the z) line, inside the _VERSION_PRERELEASE guard (handleDebug.ino). AC2: tests/test_debug_help_heaphold.py reads the real handleDebug.ino: red on the old placement (2 of 3), green 3/3; test_heaphold_hook 8/8; evaluate.py --quick 89/0; build 2.0.0-beta.397+7e31bc2 (build.bat exit 0, Build completed successfully). AC1: bench PASS by otgw-bench-tester on .61 with that build: 'h' lists H) right after z), 'l' only toggles MyDEBUG, 10 min regression clean (captures b397\menu.raw, b397\reg).
<!-- SECTION:FINAL_SUMMARY:END -->
