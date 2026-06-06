---
id: TASK-832
title: Remember interactive capture settings (pre-fill on next run)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-06-06 08:55'
updated_date: '2026-06-06 08:58'
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
- [x] #1 Interactive prompts show last-used value as [default]; pressing Enter accepts it
- [x] #2 DeviceHost, BrokerHost, BrokerPort, Topic, Username persisted to per-user JSON; password NEVER written
- [x] #3 Settings load/save are best-effort: failure to read/write never aborts the capture
- [x] #4 PowerShell payload parses clean; save+reload round-trips correctly in a smoke test
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
capture-mqtt-debug.bat now remembers interactive prompt answers and pre-fills them next run.

Added Get/Import/Save-CaptureSettings + Read-HostWithDefault. On start it loads %LOCALAPPDATA%/OTGW-capture/capture-settings.json; DeviceHost/BrokerHost/Username prompts show the last value as [default] (Enter accepts). After resolving inputs it saves DeviceHost, BrokerHost, BrokerPort, Topic, Username. The MQTT password is NEVER persisted. Load and save are best-effort (try/catch) so a missing/corrupt/unwritable file never aborts the capture; summary.txt records the saved path.

Verified: AST parse clean (1547 lines); JSON save/load round-trips (Device/Broker/Port/User); password key absent from persisted object. Script-only change.
<!-- SECTION:FINAL_SUMMARY:END -->
