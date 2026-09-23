---
id: TASK-1161
title: >-
  Capture script for Linux/macOS at feature parity with capture-mqtt-debug.bat,
  shipped with every release
status: Done
assignee:
  - '@claude'
created_date: '2026-09-23 19:11'
updated_date: '2026-09-23 20:24'
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
- [x] #1 A written feature inventory of capture-mqtt-debug.bat maps every feature to its implementation in the shell script, or to a documented platform reason why it cannot exist there
- [x] #2 The shell script runs on bash 3.2 (macOS default) and on Linux bash, requires only bash + curl + standard POSIX tools, and treats optional tools (mosquitto_sub, nc) as degrade-not-abort
- [x] #3 Validated on this machine under WSL Ubuntu against the bench gateway 192.168.88.68: a full capture run produces every output the Windows script produces, and the summary reports what was and was not captured
- [x] #4 scripts/make_release_assets.py attaches capture-otgw.sh as an individual asset and in the bundle; RELEASE_ASSETS.md and the /release skill asset count reflect it
- [x] #5 beta-prerelease.yml attaches capture-otgw.sh (TASK-1156) and the release text describes it as the Linux/macOS equivalent
- [x] #6 shellcheck (or bash -n where shellcheck is unavailable) passes on the script, and the flash-scripts-lint workflow covers it if that workflow lints shell scripts
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

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
## Feature inventory: capture-mqtt-debug.bat -> capture-otgw.sh 2.0.0

| Windows feature | Shell implementation |
|---|---|
| All 25 parameters (-DeviceHost ... -CrashlogPollSeconds), ValidateRange | Same names, case-insensitive, PS or GNU spelling; int_opt enforces the same ranges |
| Interactive prompts, [default] prefill, username prompt rule, SecureString password | read_with_default; same bound/unbound rules; read -rs for the password |
| Settings file %LOCALAPPDATA%\OTGW-capture (password never saved) | ${XDG_CONFIG_HOME:-~/.config}/otgw-capture/capture-settings.json, same keys, password never saved |
| Run folder logs/mqtt-diagnostics/yyyyMMdd-HHmmss | Same |
| summary.txt line set, toggle policy line up front | Same lines and wording, plus one Platform line |
| Telnet TcpClient read+write | Background worker: /dev/tcp fd + cat into telnet.log, FIFO for writes |
| Banner parse, enable-all / Skip / Quiet+Keep, simulators excluded, restore on exit | Same regex semantics in awk, same result strings; flipped keys via a state file |
| q + D dump with idle drain (800 ms, cap 5 s) | Same, drain measured on telnet.log growth |
| Reconnect: post-disconnect delay, reconnect delay, adaptive timeout (base+min(n-1,6), cap 20) | Same arithmetic and status lines |
| ESP.restart() marker with 512-byte overlap | Same, scanned on telnet.log byte offsets |
| mosquitto_sub: explicit path fatal, PATH, common paths, install, degrade with warning | Same; install = brew on macOS (no sudo), printed apt/dnf/pacman/zypper/apk hint on Linux |
| mosquitto_sub exit ends the capture | Same |
| Browser: Edge/Chrome headless, CDP port auto-bump, temp profile, console/exception/Log/net/FAILED/PENDING | Chrome/Chromium/Edge/Brave incl. macOS app bundles and snap; embedded stdlib python3 CDP client, same event mapping and line format; --no-sandbox retry recorded in summary |
| Crash-log worker: 30 s, crashlog + reboot_log.txt, change-only, resilient GET (10 s, 1 retry, 1.5 s), classified transport error | Same, curl exit codes classified (Timeout, ConnectFailure, ...) |
| REST snapshot after stop, curated msgids, never /api/v2/settings | Same list and format |
| Metadata: telnet.log, /api/v2/debug, /api/v2/settings, /api/v2/device/info, MAC fallback | Same order and prefer-existing rules |
| error.txt (script/mqtt/browser stderr), merged transcript, intermediates removed | Same sections and titles |
| Q key, Ctrl+C, -DurationSeconds | Same; Q polled via /dev/tty |

Deliberate non-parity, with the platform reason:
- No user-PATH mutation after a mosquitto install: Homebrew and distro packages already land on PATH.
- winget -> brew on macOS; on Linux an install hint only, because the package manager needs sudo.
- No Ctrl+Break / cmd batch-job prompt: those are cmd.exe artefacts.
- Q latency is 1 s on bash 3.x (no fractional read -t, verified on 3.2.57); 0.2 s on bash 4+. Capture itself runs in background processes and loses nothing.
- A Windows browser cannot be driven from WSL; the summary says so.

Behaviour changes against the previous capture-otgw.sh 1.x:
- Default duration is until stopped (was 10 min).
- Debug toggles are switched on by default (was passive); -SkipDebugToggles / -QuietDebugToggles restore the old behaviour.
- The browser capture is on by default (was deliberately absent); -SkipBrowserCapture turns it off.
- Output is one transcript file (was a directory to zip).
- No early abort when nothing is reachable: it keeps retrying, as the Windows script does, with a status line per attempt.

Extras on top of the Windows script: -Serial [dev] with -Baud (usb-serial.log, CR strip, timestamps, baud-mismatch warning; the Windows side has capture-usb-serial.bat) and -ProbeSeconds (probe.log).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
The Linux, WSL and macOS capture now matches the Windows one, and ships with every release. A macOS reporter on GH #682 could not capture at all before this.

What changed
- scripts/capture-otgw.sh 2.0.0 is rewritten with capture-mqtt-debug.bat as the template. It has the same options (PowerShell or GNU spelling), prompts, remembered settings, toggle policies with restore, q+D dump, reconnect logic, crash-log poller, REST snapshot, metadata resolution and single merged transcript. The headless browser capture uses an embedded stdlib-only python3 CDP client. The feature-by-feature mapping and every deliberate difference are in the notes.
- Written for bash 3.2. Read and write on one socket go through a background worker and a FIFO, because bash 3.2 has no coproc and no fractional read -t.
- Shipping: make_release_assets.py attaches it to stable releases and sets the exec bit for .sh files in the bundle. Asset counts go from 9 to 10 in the release and beta skills and in RELEASE_PROCESS.md. The beta release text describes the new flow.
- CI: flash-scripts-lint.yml runs bash -n and shellcheck on it, and now also triggers on otgw-1.x.x. It never ran on the 1.x line after the June branch change.

Verified in WSL Ubuntu 24.04 against the bench gateway (192.168.88.68). The gateway was pointed at a temporary local broker with a single-field POST, and afterwards restored to homeassistant.local:1883. The MQTT password was never touched.
- Default run: telnet (toggles on, D dump), MQTT, browser (console plus ~70 net lines), crash log, REST snapshot and metadata all present; the transcript is named from telnet metadata.
- Quiet+Keep: exactly the right toggles flipped and restored. Device state checked over telnet before and after.
- Reboot mid-capture: ESP.restart() detected, reconnected after 1 attempt, toggles re-applied, reboot_log.txt change recorded.
- Q, Ctrl+C (real SIGINT through a pty) and duration stops all leave a complete transcript, with no leftover processes or temp dirs.
- Degrade paths: no mosquitto_sub (install hint, telnet continues), unreachable host (timeouts growing 6 to 10 s), drvfs output dir, remembered prompt defaults. The password was never written to the settings file or the transcript.
- Serial through a pty: clean input gives timestamped lines with no CRs; garbage input gives the baud-mismatch warning.
- A full run under a locally built bash 3.2.57 (the macOS version) passes. shellcheck 0.9.0 clean. evaluate.py --quick 0 failed.
- The D and q dumps mask both passwords (handleDebug.ino:28/38, verified in code). The 6 password mentions in the test transcripts were all masked.

Not verified, and why
- A real Mac. BSD date, sed, awk, stty, tail and pkill, Apple bash and the python3-stub guard were checked by review only. The first macOS field capture is the real test.
- Serial against real hardware: WSL has no COM passthrough.

Known behaviour: Ctrl+C also reaches mosquitto_sub, which exits at that moment instead of being stopped by the script. It has no effect on the data.

Commits: 6d0c25b8a, 2b1c67a09.
<!-- SECTION:FINAL_SUMMARY:END -->
