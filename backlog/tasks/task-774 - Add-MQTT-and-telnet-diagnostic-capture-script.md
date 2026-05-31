---
id: TASK-774
title: Add MQTT and telnet diagnostic capture script
status: Done
assignee:
  - '@codex'
created_date: '2026-05-31 08:50'
updated_date: '2026-05-31 09:05'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Add a PowerShell diagnostic script that captures OTGW telnet debug output and MQTT broker traffic in parallel, bootstrapping mosquitto_sub on Windows when needed.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 scripts/capture-mqtt-debug.ps1 supports the requested connection, duration, output, credential, and tooling parameters.
- [x] #2 The script discovers mosquitto_sub from an explicit path, PATH, and common install locations, optionally installs Mosquitto with winget, refreshes the current PATH, and persists the user PATH when needed.
- [x] #3 The script creates a timestamped run folder with telnet.log, mqtt.log, and summary.txt, enables MQTT telnet debug only when the banner indicates it is off, and records errors/status details.
- [x] #4 scripts/README.md documents the diagnostic script briefly.
- [x] #5 PowerShell parse validation passes for the new script.
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Add a PowerShell diagnostic script that resolves or installs mosquitto_sub, opens OTGW telnet, enables MQTT debug only when the banner reports it off, and captures telnet plus MQTT logs into a timestamped folder.
2. Add a Windows batch launcher so the diagnostic can be started from cmd.exe while forwarding all arguments to the PowerShell script.
3. Document both entry points and validate parser, batch launcher, invalid-tool-path failure, and a loopback telnet banner/toggle flow.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Branch: dev
Coding agent: Codex
Implementation commit: 5072f9a4 tools: add MQTT debug capture script
Files changed: scripts/capture-mqtt-debug.ps1, scripts/capture-mqtt-debug.bat, scripts/README.md

Implemented a PowerShell diagnostic that prompts for missing hosts, resolves or installs mosquitto_sub, refreshes/persists PATH when Mosquitto is found outside PATH, captures OTGW telnet output and MQTT subscription output to a timestamped run folder, parses the telnet banner for 3 MQTT [0/1], and sends key 3 only when MQTT debug is off. Added a cmd.exe batch launcher that prefers pwsh and falls back to Windows PowerShell, forwarding all arguments to the .ps1 script.

Validation evidence:
- PowerShell parser check passed under pwsh.
- Windows PowerShell parser check passed under powershell.exe.
- Batch launcher invalid -MosquittoSubPath dry run returned the expected explicit missing-path error and wrote summary.txt.
- Batch launcher fallback path test with PATH limited to Windows PowerShell returned the same expected missing-path error through powershell.exe.
- Loopback telnet stub validation through capture-mqtt-debug.bat created telnet.log, mqtt.log, mqtt.stderr.log, and summary.txt; summary.txt recorded MQTT debug toggle action: sent key 3 because MQTT debug was off; telnet.log contained the banner and Debug MQTT: ON response.
- git diff --check passed for scripts/README.md; new scripts had no trailing whitespace.

Not run against physical OTGW hardware or a live MQTT broker in this environment.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Added scripts/capture-mqtt-debug.ps1 to capture OTGW telnet debug and MQTT broker traffic together, with mosquitto_sub discovery/install bootstrap, PATH refresh/persistence, timestamped logs, banner-based MQTT debug enablement, and summary reporting.

Added scripts/capture-mqtt-debug.bat so the diagnostic can be started from cmd.exe, and documented both PowerShell and batch entry points in scripts/README.md. Validation covered parser checks, batch launcher failure behavior, Windows PowerShell fallback, and a loopback telnet banner/toggle capture flow; live OTGW/MQTT validation remains manual.
<!-- SECTION:FINAL_SUMMARY:END -->

## Definition of Done
<!-- DOD:BEGIN -->
- [x] #1 Backlog task includes branch metadata, coding-agent metadata, implementation notes, validation evidence, and final summary.
<!-- DOD:END -->
