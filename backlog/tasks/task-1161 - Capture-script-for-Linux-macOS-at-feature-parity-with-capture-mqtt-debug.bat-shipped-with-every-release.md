---
id: TASK-1161
title: >-
  Capture script for Linux/macOS at feature parity with capture-mqtt-debug.bat,
  shipped with every release
status: In Progress
assignee:
  - '@claude'
created_date: '2026-09-23 19:11'
updated_date: '2026-09-23 19:15'
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

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Rewrite scripts/capture-otgw.sh with capture-mqtt-debug.bat as the template, bash 3.2 compatible:
   - options mirror the PowerShell ones (-DeviceHost style accepted, plus --device-host style); the old --host/--broker/--minutes/--serial/--reconnect flags stay as aliases/extras
   - interactive prompts with remembered defaults (~/.config/otgw-capture/capture-settings.json, password never saved, silent password prompt)
   - telnet on /dev/tcp fd 3, read+write: banner parse, debug toggles enable-all / --skip / --quiet+--keep with restore on exit, q+D settings dump, always-on reconnect with adaptive timeout, ESP.restart() marker
   - mosquitto_sub mirror, degrade-not-abort with install hint per platform (apt/dnf/brew)
   - crash-log poller in background: 30s, crashlog endpoint + /reboot_log.txt, change-only logging, one retry, timeout classification
   - headless browser CDP capture via an embedded python3 stdlib CDP client (console, exceptions, Log, network timings, FAILED, PENDING); Chrome/Chromium/Edge/Brave auto-detect incl. macOS app bundles; skipped with a note when python3 or a browser is missing
   - REST snapshot after the stop: device/info, otmonitor, boiler-support, curated msgids; never /api/v2/settings (credentials)
   - stop on Q, Ctrl+C or --duration-seconds; summary.txt timeline; error.txt; metadata resolution; single merged transcript-<run>-<version>-<host>-<id>.txt; intermediates removed
   - serial mode kept as an extra (Windows has capture-usb-serial.bat for that)
2. Ship it: make_release_assets.py (individual asset + bundle + RELEASE_ASSETS.md text), /release skill asset count, beta-prerelease.yml text (transcript instead of directory), any docs naming the old flags.
3. Lint: shellcheck, bash -n; flash-scripts-lint workflow coverage.
4. Validate in WSL Ubuntu against 192.168.88.68: default run, quiet+keep toggles with restore check, forced reboot mid-capture (reconnect + crashlog), MQTT mirror, browser capture, Q stop, duration stop, transcript naming. Mac-specific paths (bash 3.2, BSD date/stty) checked by static review and a bash 3.2 build if obtainable.
<!-- SECTION:PLAN:END -->
