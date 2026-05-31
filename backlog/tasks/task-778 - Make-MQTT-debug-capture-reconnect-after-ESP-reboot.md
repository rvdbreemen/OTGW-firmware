---
id: TASK-778
title: Make MQTT debug capture reconnect after ESP reboot
status: In Progress
assignee:
  - '@codex'
created_date: '2026-05-31 10:04'
updated_date: '2026-05-31 10:07'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Telnet capture in capture-mqtt-debug.bat/ps1 does not reconnect after the ESP reboots during capture. Make the telnet capture retry aggressively, wait about 5 seconds after reboot/disconnect before reconnect attempts, use a connect timeout under 3 seconds, retry about every 2 seconds until successful, and report reconnects.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Telnet capture automatically reconnects after an ESP reboot or dropped telnet connection.
- [ ] #2 After a reboot/disconnect, reconnect attempts wait about 5 seconds before the first attempt.
- [ ] #3 Each connect attempt times out in under 3 seconds and retries about every 2 seconds until successful.
- [ ] #4 Successful reconnects are reported in capture output/logging.
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Inspect current capture-mqtt-debug telnet connection and reconnect loop.\n2. Patch reconnect timing: initial connect immediately, post-disconnect/reboot wait around 5 seconds, connect timeout under 3 seconds, retry around every 2 seconds.\n3. Detect ESP.restart() reboot marker in captured telnet output and force the reconnect path.\n4. Report reconnect success in summary and console output.\n5. Validate PowerShell parsing/targeted script behavior and update AC/DoD.
<!-- SECTION:PLAN:END -->

## Definition of Done
<!-- DOD:BEGIN -->
- [ ] #1 Capture script validation is run or the exact limitation is documented.
<!-- DOD:END -->
