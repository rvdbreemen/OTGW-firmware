---
id: TASK-776
title: Fix MQTT/telnet capture interrupt and reconnect
status: In Progress
assignee:
  - '@codex'
created_date: '2026-05-31 09:46'
updated_date: '2026-05-31 09:46'
labels:
  - scripts diagnostics bug
dependencies: []
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Fix the diagnostic capture script so Ctrl+C exits cleanly and telnet logging survives OTGW reboots by reconnecting until capture is stopped.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Ctrl+C stops the capture without a PowerShell unhandled runspace exception and still writes summary.txt.
- [ ] #2 Telnet capture reconnects after the OTGW device disconnects or reboots and keeps retrying until the user stops the capture.
- [ ] #3 Local validation exercises the stop/reconnect control flow without requiring changes outside the diagnostic capture script.
<!-- AC:END -->
