---
id: TASK-835
title: Add crash-log REST polling to capture-mqtt-debug script
status: In Progress
assignee:
  - '@claude'
created_date: '2026-06-06 16:04'
updated_date: '2026-06-06 16:11'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
The mqtt-debug capture script reads the coarse MQTT reboot_reason topic (just 'Exception') but never polls the firmware's /api/v2/device/crashlog endpoint, which exposes the decoded exception (exccause + epc1/epc2/epc3/excvaddr/depc) persisted in /reboot_log.txt. For George's 1.7.0 crash-loop investigation we need that decode captured automatically. Add a crashlog poller worker mirroring the existing browser-capture runspace pattern, writing crashlog.log, merged into transcript.txt.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Script polls http://<DeviceHost>/api/v2/device/crashlog on an interval and appends JSON results to crashlog.log
- [x] #2 Poller runs as a runspace sharing the CancelFlag stop signal, started in try and drained in finally, same shape as the browser worker
- [x] #3 404/non-JSON/timeout responses are logged but never abort the capture (graceful across 1.2.0 which lacks the endpoint)
- [x] #4 crashlog.log is folded into transcript.txt and removed as an intermediate file
- [x] #5 New -SkipCrashlogCapture switch and -CrashlogPollSeconds param (default 30) exist; --help documents them
- [x] #6 Existing capture behaviour (telnet/mqtt/browser) unchanged when new flags are default
<!-- AC:END -->
