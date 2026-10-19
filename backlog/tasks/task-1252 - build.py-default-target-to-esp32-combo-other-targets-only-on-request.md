---
id: TASK-1252
title: 'build.py: default --target to esp32-combo; other targets only on request'
status: Done
assignee:
  - '@claude'
created_date: '2026-10-18 20:16'
updated_date: '2026-10-19 04:49'
labels:
  - build
dependencies: []
priority: medium
ordinal: 385000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
build.py --target defaults to 'all' (esp32, esp32-classic, esp32-combo). Maintainer request 2026-10-18: a plain build (build.bat/build.sh/python build.py) builds only esp32-combo; esp32, esp32-classic and all only when asked with --target. The release skill keeps building all three by passing --target all explicitly.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 build.py without --target builds only esp32-combo
- [x] #2 --target esp32 / esp32-classic / all still work
- [x] #3 A test fails on the old default and passes on the new one
- [x] #4 Help text, BUILD.md, CLAUDE.md/AGENTS.md and the release skill describe the new default; the release skill passes --target all
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-19 evidence (session 8b000f87, continuing the work of session 8fe0651d):
- AC1: plain build.bat with no arguments (log scratchpad/build-1252-plain2.log): 'Target(s): ESP32-S3 Combo', only safeboot and esp32-combo processed (0 lines for the esp32 or esp32-classic envs), esp32-combo SUCCESS 00:04:20 (firmware) and 00:00:19 (filesystem), 'Collected 4 artifact(s)', 'Created distribution zip: OTGW-firmware-esp32-combo-2.0.0-alpha.436+56b64a6-flash.zip (4056.6 KB)', exit 0.
- Two earlier plain builds failed for reasons outside this change. 2026-10-18 22:22: bootloader.bin and partitions.bin vanished from .pio/build/esp32-combo during the link, so no merged bin; a second PlatformIO run auto-cleaning .pio/build is the likely cause, but which process ran it was not identified. 2026-10-19 06:07: WinError 32, a transient lock on build/OTGW-firmware-esp32-combo.ino.bin during the rename (the file was unlocked a minute later). Review agents confirmed args.target is read only at build.py:2928-2931 and build.py never reads sys.argv, so a plain build and --target esp32-combo run identical code.
- AC3: tests/test_build.py TestDefaultTarget was red on the old default (AssertionError: 'all' != 'esp32-combo', session 8fe0651d) and is green now: Ran 11 tests, OK. tests/test_adr_governance.py: Ran 15 tests, OK (CLAUDE.md/AGENTS.md binding block still identical).
- AC4: build.py docstring, epilog and --target help; docs/guides/BUILD.md; CLAUDE.md (build commands, canonical build line, origin/dev push gate: add --target all when HAS_*-gated code, boards.h/platform headers or platformio.ini change); AGENTS.md build commands; release skill (--target all on both build lines and in the rules list); docs/process/RELEASE_PROCESS.md (3 places); docs/guides/ESP_ABSTRACTION.md; docs/MANUAL.md; developer manual EN ch08 and NL h08. Found by a 4-lens review workflow (30 agents, each finding checked by 2 skeptics: 9 confirmed, 4 refuted).
- evaluate.py --quick: 72 passed, 0 warnings, 0 failed.
- AC2 open: waiting for a --target all build.

- AC2: build.bat --target all on the pushed commit 58a75af (log scratchpad/build-1252-all.log): 'Target(s): ESP32-S3, ESP32-S3 Classic, ESP32-S3 Combo'; esp32 SUCCESS 00:04:27 + 00:00:26, esp32-classic SUCCESS 00:04:26 + 00:00:26, esp32-combo SUCCESS 00:04:25 + 00:00:22; three zips: esp32-otgw32 (3999.7 KB), esp32-classic (3946.7 KB), esp32-combo (4056.6 KB), exit 0. --target esp32 and --target esp32-classic run the same per-target code as one entry of 'all' (build.py:2928-2931) and parse correctly (TestDefaultTarget.test_other_targets_still_selectable); they were not built on their own.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
A plain build.bat / build.sh / python build.py now builds esp32-combo only; esp32, esp32-classic and all still build when passed with --target. The parser moved into build_arg_parser() so tests/test_build.py can check the default (red on the old 'all', green now: 11 OK). Evidence: AC1 plain build.bat built only esp32-combo and made its zip (exit 0); AC2 --target all on 58a75af built all three targets with three zips; AC3 TestDefaultTarget red then green; AC4 help text, BUILD.md, CLAUDE.md, AGENTS.md, the release skill (--target all), RELEASE_PROCESS.md, ESP_ABSTRACTION.md, MANUAL.md and the EN/NL developer manuals updated after a 30-agent review (9 confirmed findings, all fixed). evaluate.py --quick 72/0/0; test_adr_governance 15 OK. Not verified: --target esp32 and --target esp32-classic were not built on their own (same code path as one entry of 'all'). Two earlier plain-build failures were outside this change: .pio/build emptied during a link by an unidentified second PlatformIO run (2026-10-18), and a transient WinError 32 lock on the renamed bin (2026-10-19). Commit 58a75aff7.
<!-- SECTION:FINAL_SUMMARY:END -->
