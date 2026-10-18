---
id: TASK-1247
title: Persist the doRestart() reason in reboot_log.txt
status: To Do
assignee: []
created_date: '2026-10-18 08:18'
labels:
  - diagnostics
  - reboot
dependencies: []
priority: high
ordinal: 243000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
reboot_log.txt stores only the ESP reset reason. Every planned restart (WiFi reconnect failures, nightly restart, OTA, telnet operator, deferred reboot) shows up as the same 'Software/System restart (4)', so a field log cannot tell which restart fired. Found while triaging GH #682 / TASK-1246: two ~72 min network hangs on 1.7.7 each end in a restart (4), and the cause of that restart is the key question.
Approach: doRestart() writes its reason (magic + up to 47 chars) to RTC user memory, which survives ESP.restart(). updateRebootLog() reads and clears it at boot and appends it to the reboot-cause line when the reset reason is a software restart. RTC user slots: 64..95 belong to eboot (OTA command), 96..97 to the WiFi portal reset counter; this uses 100..112.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 After a doRestart() the next reboot_log.txt entry ends with the restart reason text (bench: trigger via the telnet operator reboot and read /reboot_log.txt; old build shows no reason)
- [ ] #2 A power-on or external/watchdog reset shows no stale reason (magic checked and cleared on every boot)
- [ ] #3 esp8266 build passes and evaluate.py --quick is green
<!-- AC:END -->
