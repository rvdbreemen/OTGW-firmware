---
id: TASK-778
title: Make MQTT debug capture reconnect after ESP reboot
status: Done
assignee:
  - '@codex'
created_date: '2026-05-31 10:04'
updated_date: '2026-05-31 10:11'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Telnet capture in capture-mqtt-debug.bat/ps1 does not reconnect after the ESP reboots during capture. Make the telnet capture retry aggressively, wait about 5 seconds after reboot/disconnect before reconnect attempts, use a connect timeout under 3 seconds, retry about every 2 seconds until successful, and report reconnects.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Telnet capture automatically reconnects after an ESP reboot or dropped telnet connection.
- [x] #2 After a reboot/disconnect, reconnect attempts wait about 5 seconds before the first attempt.
- [x] #3 Each connect attempt times out in under 3 seconds and retries about every 2 seconds until successful.
- [x] #4 Successful reconnects are reported in capture output/logging.
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Inspect current capture-mqtt-debug telnet connection and reconnect loop.
2. Patch reconnect timing: initial connect immediately, post-disconnect/reboot wait around 5 seconds, connect timeout under 3 seconds, retry around every 2 seconds.
3. Detect ESP.restart() reboot marker in captured telnet output and force the reconnect path.
4. Report reconnect success in summary and console output.
5. Validate PowerShell parsing/targeted script behavior and update AC/DoD.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Branch: dev
Coding agent: Codex
Files changed: scripts/capture-mqtt-debug.ps1

Implementation notes:
- Telnet connect timeout changed from 5 seconds to 2 seconds.
- Added explicit 5 second post-disconnect/reboot backoff before the first reconnect attempt.
- Kept failed connect retries on a 2 second cadence until a connection succeeds or capture stops.
- Added ESP.restart() marker detection using the reboot line shown in the reported capture output; when seen, the script closes the current telnet client and enters the reconnect path.
- Reconnect status is written to summary.txt and shown on the console.

Validation:
- PowerShell parser ParseFile on capture-mqtt-debug.ps1: OK.
- powershell -NoProfile -ExecutionPolicy Bypass -File .\capture-mqtt-debug.ps1 -Help: OK.
- cmd /c .\capture-mqtt-debug.bat --help: OK.
- git diff --check -- .\capture-mqtt-debug.ps1: OK, with existing CRLF normalization warning only.
- Regex check against the reported "calling ESP.restart()" line: OK.

Limitation:
- No live ESP reboot/MQTT broker capture run was performed in this session.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Implemented aggressive telnet reconnect handling for capture-mqtt-debug:
- sub-3-second telnet connect timeout,
- 5-second wait after disconnect/reboot before reconnect attempts,
- 2-second retry cadence until reconnect,
- forced reconnect when the captured log contains ESP.restart(),
- console and summary reporting for disconnects, attempts, and successful reconnects.

Validated script parsing, PowerShell help, batch launcher help, diff whitespace, and the reported reboot marker regex.
<!-- SECTION:FINAL_SUMMARY:END -->

## Definition of Done
<!-- DOD:BEGIN -->
- [x] #1 Capture script validation is run or the exact limitation is documented.
<!-- DOD:END -->
