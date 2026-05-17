---
id: TASK-621
title: >-
  fix(build): auto-init missing git submodules in build.py
  (SimpleTelnet/OpenTherm)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-05-17 21:55'
updated_date: '2026-05-17 21:55'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
build.py fails out-of-the-box in fresh clones / web-container sessions because the vendored libraries src/libraries/SimpleTelnet and src/libraries/OpenTherm are git submodules that are not checked out (the clone ran without 'git submodule update --init'). Compilation aborts with 'fatal error: SimpleTelnet.h: No such file or directory'. build.py already self-bootstraps arduino-cli and the ESP8266 core; it should likewise detect empty/missing submodule libraries and run 'git submodule update --init' for them so a fresh checkout builds with no manual step. Must degrade gracefully when not a git checkout or git is unavailable.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 build.py detects when build-critical submodule libraries (src/libraries/SimpleTelnet, src/libraries/OpenTherm) are missing their headers and runs 'git submodule update --init' for them automatically before the firmware compile step
- [ ] #2 When the working copy is not a git checkout OR git is unavailable, build.py prints a clear actionable warning and continues (does not crash) instead of failing only later at the compiler error
- [ ] #3 When submodules are already present, the new step is a fast no-op with no spurious git calls beyond a cheap presence check
- [ ] #4 python build.py --firmware exits 0 on a fresh checkout with uninitialized submodules without any manual 'git submodule update --init'
- [ ] #5 python evaluate.py --quick shows no new failures
<!-- AC:END -->
