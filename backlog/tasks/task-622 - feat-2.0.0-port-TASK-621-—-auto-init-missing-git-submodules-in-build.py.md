---
id: TASK-622
title: 'feat-2.0.0: port TASK-621 — auto-init missing git submodules in build.py'
status: Done
assignee:
  - '@claude'
created_date: '2026-05-18 04:16'
updated_date: '2026-05-18 04:20'
labels: []
dependencies: []
ordinal: 38000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Port the dev-line submodule self-heal (dev TASK-621, PR #594) to the 2.0.0 build.py. Same root cause: a plain git clone (fresh container / web session) omits src/libraries/SimpleTelnet and src/libraries/OpenTherm git submodules (.gitmodules on 2.0.0 is identical to dev for these), so build.py fails late with 'fatal error: SimpleTelnet.h: No such file or directory'. TASK-542 only fixed .gitmodules registration on 2.0.0; build.py still has no self-heal. The 2.0.0 build.py is multi-target (~2400 lines, esp8266/esp32) with a different main() structure, but its print_*/run_command helpers are byte-identical to dev, so ensure_submodules() ports verbatim — only the insertion point differs.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 build.py detects when build-critical submodule libraries (src/libraries/SimpleTelnet, src/libraries/OpenTherm) are missing their headers and runs 'git submodule update --init' for them automatically, before any backend (arduino-cli/PlatformIO) or target setup
- [x] #2 When the working copy is not a git checkout OR git is unavailable, build.py prints a clear actionable warning and continues (no crash) instead of failing later at the compiler error
- [x] #3 When submodules are already present, the new step is a fast no-op with no git invocation
- [x] #4 python build.py --target esp8266 --firmware exits 0 on a fresh checkout with uninitialized submodules without any manual 'git submodule update --init'
- [x] #5 python evaluate.py --quick shows no new failures
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Port SUBMODULE_LIBS + _submodule_has_sources + ensure_submodules verbatim from dev build.py (helpers/run_command are byte-identical on 2.0.0)
2. Define the block just before def setup_arduino_config(project_dir, target_names) (parity with dev placement)
3. Call ensure_submodules(project_dir) in main() right after the clean-handling block, before target-list resolution / backend setup (universal: needed for both arduino-cli and PlatformIO, both targets)
4. Verify: git submodule deinit -f both libs; python build.py --target esp8266 --firmware exits 0 with no manual init; re-run with submodules present = fast no-op (no git call); non-git temp dir = warn + continue; python evaluate.py --quick no new failures
5. Tooling-only (top-level build.py) -> no prerelease bump per 2.0.0 versioning policy; push new branch, open draft PR vs feature-dev-2.0.0-otgw32-esp32-sat-support
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Ported verbatim from dev TASK-621 (helpers/run_command byte-identical on 2.0.0). Block defined before def setup_arduino_config; ensure_submodules(project_dir) called in main() after clean-handling, before target/backend setup (universal for arduino-cli + PlatformIO, both targets).

Verified in /home/user/OTGW-firmware-2.0.0 (worktree on claude/port-task-621-submodule-selfheal):
- AC1/AC4: worktree had uninitialized submodules; python build.py --target esp8266 --firmware -> self-heal ran git submodule update --init, ESP8266 PlatformIO build BUILD200_EXIT=0, OTGW-firmware-esp8266-2.0.0-alpha.35+a2ffeb7.ino.bin (0.81 MB).
- AC2: non-git temp dir -> "Not a git checkout" warning, returns normally (no crash).
- AC3: submodules present -> "Submodule libraries present", zero git calls.
- AC5: evaluate.py --quick = 59 passed, 1 failed, 1 warn, 97.1%. The 1 fail is the pre-existing [PROGMEM] "Found 2 PROGMEM violations" 2.0.0 baseline debt (TASK-609/TASK-619) in firmware C/C++; diff vs base adds ZERO firmware lines (build.py only) -> no NEW failures.

Tooling-only (top-level build.py) -> no prerelease bump per 2.0.0 versioning policy (CLAUDE.md L299-303). Cross-ref dev PR #594 / dev TASK-621.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
feat-2.0.0: port TASK-621 — auto-init missing git submodules in build.py

Ports the dev-line submodule self-heal (TASK-621, PR #594) to the 2.0.0
multi-target build.py. Same root cause on 2.0.0: a plain git clone omits
src/libraries/SimpleTelnet and src/libraries/OpenTherm (.gitmodules
identical to dev), so build.py failed late with
`fatal error: SimpleTelnet.h: No such file or directory`. TASK-542 only
fixed .gitmodules registration; build.py had no self-heal.

The 2.0.0 build.py print_*/run_command helpers are byte-identical to
dev, so ensure_submodules() (+ SUBMODULE_LIBS + _submodule_has_sources)
ports verbatim. Only the insertion point is adapted: the block sits
before setup_arduino_config(); ensure_submodules(project_dir) is called
in main() right after clean-handling and before target/backend setup, so
it covers both the arduino-cli and PlatformIO paths and both targets.

Verification (esp8266 target, fresh uninitialised-submodule state):
- python build.py --target esp8266 --firmware exits 0 with no manual
  init; ESP8266 binary produced (0.81 MB).
- Non-git dir: warns and continues, no exception.
- Present: "Submodule libraries present", no git invocation.
- evaluate.py --quick: 1 pre-existing PROGMEM baseline failure only
  (TASK-609/619); diff adds zero firmware lines -> no new failures.

Scope: tooling-only (top-level build.py) -> no prerelease bump per the
2.0.0 versioning policy. No ADR (minor robustness fix within build.py's
existing self-bootstrapping pattern). Pushed to branch
claude/port-task-621-submodule-selfheal; draft PR vs
feature-dev-2.0.0-otgw32-esp32-sat-support.
<!-- SECTION:FINAL_SUMMARY:END -->
