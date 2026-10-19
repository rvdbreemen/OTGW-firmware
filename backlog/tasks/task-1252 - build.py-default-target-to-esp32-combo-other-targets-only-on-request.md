---
id: TASK-1252
title: 'build.py: default --target to esp32-combo; other targets only on request'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-10-18 20:16'
updated_date: '2026-10-19 04:32'
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
- [ ] #2 --target esp32 / esp32-classic / all still work
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
<!-- SECTION:NOTES:END -->
