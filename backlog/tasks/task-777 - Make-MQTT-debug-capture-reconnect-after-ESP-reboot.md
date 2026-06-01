---
id: TASK-777
title: Make MQTT debug capture reconnect after ESP reboot
status: Done
assignee:
  - '@codex'
created_date: '2026-05-31 10:03'
updated_date: '2026-06-01 20:58'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Telnet capture in capture-mqtt-debug.bat/ps1 does not reconnect after the ESP reboots during capture. Make the telnet capture retry aggressively, wait about 5 seconds after reboot/disconnect before reconnect attempts, use a connect timeout under 3 seconds, retry about every 2 seconds until restored, and report reconnects.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Telnet capture automatically reconnects after an ESP reboot or dropped telnet connection.
- [x] #2 After a reboot/disconnect, reconnect attempts wait about 5 seconds before the first attempt.
- [x] #3 Each connect attempt times out in under 3 seconds and retries about every 2 seconds until successful.
- [x] #4 Successful reconnects are reported in capture output/logging.
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Closed as delivered via duplicate TASK-778. TASK-778 contains the actual implementation notes, validation, and final summary for the MQTT debug capture reconnect work, and commit 1b92a681 (`fix(scripts): reconnect capture telnet TASK-778`) delivered the script changes. Keeping this task open was backlog bookkeeping drift, not remaining implementation work.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Closed as delivered. The MQTT debug capture reconnect behavior requested here was implemented and validated under duplicate TASK-778, including reconnect-after-reboot handling, 5-second post-disconnect wait, sub-3-second connect timeout, roughly 2-second retry cadence, and reconnect reporting. No additional code change was required for TASK-777 itself.
<!-- SECTION:FINAL_SUMMARY:END -->

## Definition of Done
<!-- DOD:BEGIN -->
- [x] #1 Capture script validation is run or the exact limitation is documented.
<!-- DOD:END -->
