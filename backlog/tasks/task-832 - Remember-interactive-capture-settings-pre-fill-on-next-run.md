---
id: TASK-832
title: Remember interactive capture settings (pre-fill on next run)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-06-06 08:55'
updated_date: '2026-06-06 08:56'
labels:
  - tooling
  - diagnostics
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
capture-mqtt-debug.bat prompts for DeviceHost, BrokerHost and MQTT username every run. Persist the answers to a per-user JSON (%LOCALAPPDATA%/OTGW-capture/capture-settings.json) and pre-fill the prompts next time with [default]=last value (Enter accepts). Never persist the MQTT password (secret). Saved on every run (interactive or param) so prefill stays current.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Interactive prompts show last-used value as [default]; pressing Enter accepts it
- [ ] #2 DeviceHost, BrokerHost, BrokerPort, Topic, Username persisted to per-user JSON; password NEVER written
- [ ] #3 Settings load/save are best-effort: failure to read/write never aborts the capture
- [ ] #4 PowerShell payload parses clean; save+reload round-trips correctly in a smoke test
<!-- AC:END -->
