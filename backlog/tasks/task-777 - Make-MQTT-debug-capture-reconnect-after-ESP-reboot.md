---
id: TASK-777
title: Make MQTT debug capture reconnect after ESP reboot
status: To Do
assignee: []
created_date: '2026-05-31 10:03'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Telnet capture in capture-mqtt-debug.bat/ps1 does not reconnect after the ESP reboots during capture. Make the telnet capture retry aggressively, wait about 5 seconds after reboot/disconnect before reconnect attempts, use a connect timeout under 3 seconds, retry about every 2 seconds until restored, and report reconnects.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Telnet capture automatically reconnects after an ESP reboot or dropped telnet connection.
- [ ] #2 After a reboot/disconnect, reconnect attempts wait about 5 seconds before the first attempt.
- [ ] #3 Each connect attempt times out in under 3 seconds and retries about every 2 seconds until successful.
- [ ] #4 Successful reconnects are reported in capture output/logging.
<!-- AC:END -->

## Definition of Done
<!-- DOD:BEGIN -->
- [ ] #1 Capture script validation is run or the exact limitation is documented.
<!-- DOD:END -->
