---
id: TASK-776
title: Fix MQTT/telnet capture interrupt and reconnect
status: In Progress
assignee:
  - '@codex'
created_date: '2026-05-31 09:46'
updated_date: '2026-05-31 09:55'
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
- [x] #1 Ctrl+C stops the capture without a PowerShell unhandled runspace exception and still writes summary.txt.
- [x] #2 Telnet capture reconnects after the OTGW device disconnects or reboots and keeps retrying until the user stops the capture.
- [x] #3 Local validation exercises the stop/reconnect control flow without requiring changes outside the diagnostic capture script.
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Replace the PowerShell scriptblock Ctrl+C handler with a runspace-free .NET cancel flag that the capture loop can poll.
2. Refactor telnet setup into connect/read/disconnect helpers and retry after disconnects or failed connects until stop or duration expiry.
3. Keep MQTT capture independent so mosquitto_sub continues while telnet reconnects.
4. Validate parser behavior and local stop/reconnect flow with loopback test fixtures.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Branch: dev
Coding agent: Codex
Files changed: scripts/capture-mqtt-debug.ps1

Implemented a runspace-free .NET Ctrl+C cancel flag so Console.CancelKeyPress no longer invokes a PowerShell scriptblock from the console signal thread. Refactored telnet capture into reconnectable helpers: the MQTT subscriber starts independently, telnet connect/read failures are logged, disconnected sockets are closed, and the loop retries telnet every 2 seconds until Ctrl+C, duration expiry, or mqtt process exit.

Validation evidence:
- pwsh parser check passed for scripts/capture-mqtt-debug.ps1.
- Windows PowerShell parser check passed for scripts/capture-mqtt-debug.ps1.
- Embedded cancel handler validation invoked the handler from a non-PowerShell thread, confirmed the stop flag was set, and confirmed the console cancel event was canceled.
- Loopback telnet validation connected to a local listener, enabled MQTT debug from 3 MQTT [0], survived a forced disconnect, reconnected to a second listener reporting 3 MQTT [1], captured after-reconnect in telnet.log, and stopped by duration with summary.txt written.
- git diff --check passed for scripts/capture-mqtt-debug.ps1.
<!-- SECTION:NOTES:END -->
