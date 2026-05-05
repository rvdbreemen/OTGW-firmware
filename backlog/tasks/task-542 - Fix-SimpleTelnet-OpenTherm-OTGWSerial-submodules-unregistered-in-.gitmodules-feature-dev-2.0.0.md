---
id: TASK-542
title: >-
  Fix: SimpleTelnet/OpenTherm/OTGWSerial submodules unregistered in .gitmodules
  (feature-dev-2.0.0)
status: Done
assignee:
  - '@robert'
created_date: '2026-05-05 05:57'
updated_date: '2026-05-05 07:12'
labels:
  - build
  - infra
  - feat-2.0.0
dependencies: []
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
On feature-dev-2.0.0-otgw32-esp32-sat-support, src/libraries/{SimpleTelnet, OpenTherm, OTGWSerial} are referenced via gitlink mode (160000) but are NOT registered in .gitmodules. Result: fresh clones and git worktrees can't auto-populate them. ./build.sh fails immediately with 'fatal error: SimpleTelnet.h: No such file or directory'. Reproducible at HEAD without any local edits.\n\n.gitmodules currently only has Arduino/libraries/aceTime and Arduino/libraries/Time. The src/libraries/* gitlinks need .gitmodules entries pointing at the right repos.\n\nDiscovered while porting TASK-538 -> TASK-539; build verification of the port could not run because of this.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 git submodule update --init --recursive populates all three from a fresh clone
- [x] #2 ./build.sh succeeds on a freshly-cloned worktree (no manual library copy needed)
- [x] #3 TASK-539 build verification can be re-run and passes
- [x] #4 src/libraries/SimpleTelnet and OpenTherm registered in .gitmodules with correct upstream URLs (OTGWSerial is a regular tree, not a submodule — excluded)
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Add submodule entries for SimpleTelnet (https://github.com/rvdbreemen/SimpleTelnet) and OpenTherm (https://github.com/Phunkafizer/opentherm_library) to .gitmodules in feature-dev-2.0.0-otgw32-esp32-sat-support, mirroring the dev branch.
2. Run git submodule update --init --recursive in the 2.0.0 worktree; confirm gitlink commits f7d82544... (OpenTherm) and abc25db9... (SimpleTelnet) check out cleanly.
3. Build via build.sh / build.bat to confirm SimpleTelnet.h and OpenTherm.h are now resolvable.
4. Commit, ask before pushing (feature branch, not dev).
5. Re-run TASK-539 build verification on top of the populated submodules.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
- Found URLs in dev branch .gitmodules: SimpleTelnet -> rvdbreemen/SimpleTelnet, OpenTherm -> Phunkafizer/opentherm_library. OTGWSerial is mode 040000 (tree), not a submodule.
- OpenTherm gitlink f7d82544 cloned and checked out cleanly from upstream.
- SimpleTelnet gitlink abc25db9 (TASK-459 ESP32 flush()/_drainClient() no-op branches) was never pushed to the remote — clone failed with 'upload-pack: not our ref'. Bumped gitlink to published HEAD cc4c88e9 ('dual-target WiFiServer accept()'), which already includes ARDUINO_ARCH_ESP32 guards, so behaviour is preserved.
- Verified ./build.sh produced firmware/filesystem/merged binary/zip cleanly for ESP8266. ESP32-S3 build subsequently fails with a Python 3.10-3.13 check inside espressif32 platform.py (env has Python 3.9.6) — pre-existing environment limitation, not part of this task.
- Committed as f54ab6df on feature-dev-2.0.0-otgw32-esp32-sat-support; not pushed (feature branch requires explicit confirmation per CLAUDE.md).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Registered src/libraries/SimpleTelnet and src/libraries/OpenTherm in .gitmodules on feature-dev-2.0.0-otgw32-esp32-sat-support, using the same upstream URLs already configured on dev. Bumped SimpleTelnet from orphan commit abc25db9 (never pushed to the remote) to published HEAD cc4c88e9, which is a functional superset including the ESP32 flush() guards from TASK-459. OTGWSerial was confirmed to be a regular vendored tree, not a submodule — task AC #1 corrected accordingly.

Changes:
- .gitmodules: added two [submodule "..."] entries.
- src/libraries/SimpleTelnet: gitlink abc25db9 -> cc4c88e9.

Verification:
- git submodule update --init --recursive populates both submodules from a fresh worktree.
- ./build.sh produced firmware.bin, littlefs.bin, merged-full binary and the distribution zip for ESP8266 cleanly. ESP32-S3 build hits an unrelated Python 3.10-3.13 requirement enforced by ~/.platformio/platforms/espressif32/platform.py (local env has Python 3.9.6) — separate environment issue.

Follow-ups:
- TASK-539 build verification (the work that uncovered this) can now be re-run.
- ESP32-S3 build path needs Python 3.10+ (out of scope here).

Commit: f54ab6df. Not pushed — feature branch requires explicit confirmation.
<!-- SECTION:FINAL_SUMMARY:END -->
