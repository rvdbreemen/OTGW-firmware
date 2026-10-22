---
id: TASK-1299
title: >-
  capture-mqtt-debug: the telnet log stops at the device reboot and never
  reconnects (47 s of telnet in a 1 h 44 capture)
status: To Do
assignee: []
created_date: '2026-10-22T12:00:00Z'
labels:
  - tooling
  - capture
  - bug
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 424000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
gruijter's bundle 2026-10-22 (20261022-093809.zip): browser.log runs 09:38:09 to 11:22:33 and mqtt.log 89584 lines, but telnet.log has 460 lines from 09:38:11 to 09:38:58 and ends when the device rebooted for the firmware upgrade; summary.txt lists TelnetReconnectDelayMilliseconds 500 and an adaptive timeout, yet no reconnect happened. The SAT run therefore has no debug lines at all. Reproduce on the bench: start the capture, POST /safeboot + upload a firmware, check that telnet.log resumes after the two reboots; fix the reconnect loop (scripts/capture-mqtt-debug.* / capture-otgw.sh) and add a line to the transcript when a reconnect happens.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Bench: a capture spanning a safeboot firmware update resumes telnet after both reboots (transcript shows the reconnect lines)
- [ ] #2 summary.txt reports telnet gaps (start/end of each disconnect) so a reporter sees when the log is incomplete
<!-- AC:END -->
