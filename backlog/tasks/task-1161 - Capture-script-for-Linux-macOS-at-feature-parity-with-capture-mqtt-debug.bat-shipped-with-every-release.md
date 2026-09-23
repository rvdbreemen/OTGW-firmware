---
id: TASK-1161
title: >-
  Capture script for Linux/macOS at feature parity with capture-mqtt-debug.bat,
  shipped with every release
status: To Do
assignee: []
created_date: '2026-09-23 19:11'
labels:
  - feature
  - tooling
dependencies: []
references:
  - scripts/capture-mqtt-debug.bat
  - scripts/capture-otgw.sh
  - scripts/make_release_assets.py
  - 'https://github.com/rvdbreemen/OTGW-firmware/issues/682'
priority: high
ordinal: 231000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
capture-mqtt-debug.bat (Windows, PowerShell inside a .bat) is the capture that nearly every 1.x bug was found with. The Linux/WSL/macOS counterpart scripts/capture-otgw.sh (685 lines vs 2517) was written separately and does not match its feature set. It also does not ship with stable releases at all: scripts/make_release_assets.py neither attaches it nor puts it in the bundle. The beta workflow only started attaching it with TASK-1156.

Goal: a Linux/macOS capture that uses the Windows script as its template and matches it feature for feature, attached by default to both beta and stable releases. Requested by the maintainer on 2026-09-23 after a macOS tester on GH #682 could not produce a capture.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 A written feature inventory of capture-mqtt-debug.bat maps every feature to its implementation in the shell script, or to a documented platform reason why it cannot exist there
- [ ] #2 The shell script runs on bash 3.2 (macOS default) and on Linux bash, requires only bash + curl + standard POSIX tools, and treats optional tools (mosquitto_sub, nc) as degrade-not-abort
- [ ] #3 Validated on this machine under WSL Ubuntu against the bench gateway 192.168.88.68: a full capture run produces every output the Windows script produces, and the summary reports what was and was not captured
- [ ] #4 scripts/make_release_assets.py attaches capture-otgw.sh as an individual asset and in the bundle; RELEASE_ASSETS.md and the /release skill asset count reflect it
- [ ] #5 beta-prerelease.yml attaches capture-otgw.sh (TASK-1156) and the release text describes it as the Linux/macOS equivalent
- [ ] #6 shellcheck (or bash -n where shellcheck is unavailable) passes on the script, and the flash-scripts-lint workflow covers it if that workflow lints shell scripts
<!-- AC:END -->
