---
id: TASK-621
title: >-
  fix(build): auto-init missing git submodules in build.py
  (SimpleTelnet/OpenTherm)
status: Done
assignee:
  - '@claude'
created_date: '2026-05-17 21:55'
updated_date: '2026-05-17 21:59'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
build.py fails out-of-the-box in fresh clones / web-container sessions because the vendored libraries src/libraries/SimpleTelnet and src/libraries/OpenTherm are git submodules that are not checked out (the clone ran without 'git submodule update --init'). Compilation aborts with 'fatal error: SimpleTelnet.h: No such file or directory'. build.py already self-bootstraps arduino-cli and the ESP8266 core; it should likewise detect empty/missing submodule libraries and run 'git submodule update --init' for them so a fresh checkout builds with no manual step. Must degrade gracefully when not a git checkout or git is unavailable.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 build.py detects when build-critical submodule libraries (src/libraries/SimpleTelnet, src/libraries/OpenTherm) are missing their headers and runs 'git submodule update --init' for them automatically before the firmware compile step
- [x] #2 When the working copy is not a git checkout OR git is unavailable, build.py prints a clear actionable warning and continues (does not crash) instead of failing only later at the compiler error
- [x] #3 When submodules are already present, the new step is a fast no-op with no spurious git calls beyond a cheap presence check
- [x] #4 python build.py --firmware exits 0 on a fresh checkout with uninitialized submodules without any manual 'git submodule update --init'
- [x] #5 python evaluate.py --quick shows no new failures
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Add SUBMODULE_LIBS constant + ensure_submodules(project_dir) to build.py
2. Presence check = directory contains any .h (rglob); skip git entirely when present
3. When missing: guard for non-git checkout and missing git binary (warn+continue, no crash); else run git submodule update --init <paths> via run_command(check=False); re-verify and warn if still missing
4. Call ensure_submodules() in main() after clean-handling, before install_arduino_cli
5. Verify: clear submodules, python build.py --firmware exits 0 with no manual init; re-run with submodules present = fast no-op; python evaluate.py --quick no new failures
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Implemented ensure_submodules() in build.py (SUBMODULE_LIBS + _submodule_has_sources helper), called in main() after clean-handling, before install_arduino_cli.

Verified:
- AC1/AC4: deinit src/libraries/{SimpleTelnet,OpenTherm}; python build.py --firmware -> self-heal ran git submodule update --init, firmware compiled, BUILD_EXIT=0 (OTGW-firmware-1.5.1-beta.8 .ino.bin, 70% flash).
- AC2: ensure_submodules() against a non-git temp dir -> warns "Not a git checkout", returns normally (no crash/exit).
- AC3: submodules present -> prints "Submodule libraries present", zero git calls (fast no-op).
- AC5: python evaluate.py --quick -> 34 passed, 0 failed, 0 warnings, 100% (build.py is not in evaluate.py scope; no new failures).

Tooling-only change (top-level build.py) -> exempt from prerelease bump per CLAUDE.md versioning policy.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
fix(build): auto-init missing git submodules in build.py

Problem: A plain `git clone` (fresh container / web session) omits the
src/libraries/SimpleTelnet and src/libraries/OpenTherm git submodules, so
`python build.py` failed late and cryptically with
`fatal error: SimpleTelnet.h: No such file or directory`.

Change: build.py now self-heals, consistent with how it already
bootstraps arduino-cli and the ESP8266 core. New ensure_submodules()
step (called early in main(), after clean-handling) detects build-critical
submodule libraries that are missing their headers and runs
`git submodule update --init <paths>`. Degrades gracefully with a clear,
actionable warning (no crash) when the tree is not a git checkout or
`git` is unavailable; a fast presence-only no-op when sources exist.

Verification:
- Fresh state (submodules deinit'd): `python build.py --firmware` exits 0
  with no manual init; firmware binary produced (70% flash).
- Non-git dir: warns and continues, no exception.
- Present: "Submodule libraries present", no git invocation.
- `python evaluate.py --quick`: 34 passed, 0 failed, 100%.

Scope: tooling-only (top-level build.py); no firmware/library source
change, so no prerelease bump per the versioning policy. No ADR — minor
robustness fix within build.py's existing self-bootstrapping pattern.
<!-- SECTION:FINAL_SUMMARY:END -->
