---
id: TASK-926
title: >-
  Harden capture-mqtt-debug crashlog poller: longer timeout, retry, classified
  errors
status: In Progress
assignee:
  - '@claude'
created_date: '2026-06-24 20:31'
updated_date: '2026-06-24 20:35'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Field capture (1.7.0-beta.34) showed ~15% of crashlog polls logged as 'request failed: operation has timed out' while the device was healthy. Root cause: ESP8266 single cooperative loop briefly blocks (firmware/files hex scan 2.4s, settings flash write 0.25s); the 30s crashlog poller's 5s timeout with no retry loses the socket-accept race. Make the poller resilient so it stops producing false failures.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Crashlog/reboot poll timeout raised to 10s
- [x] #2 Each poll retried once with short backoff before logging a failure
- [x] #3 Failure line classifies the WebException status (timeout vs connection refused vs other)
- [x] #4 No new sustained HTTP load added to the device
<!-- AC:END -->
