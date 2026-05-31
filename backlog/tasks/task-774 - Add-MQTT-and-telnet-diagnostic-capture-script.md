---
id: TASK-774
title: Add MQTT and telnet diagnostic capture script
status: In Progress
assignee:
  - '@codex'
created_date: '2026-05-31 08:50'
updated_date: '2026-05-31 08:51'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Add a PowerShell diagnostic script that captures OTGW telnet debug output and MQTT broker traffic in parallel, bootstrapping mosquitto_sub on Windows when needed.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 scripts/capture-mqtt-debug.ps1 supports the requested connection, duration, output, credential, and tooling parameters.
- [ ] #2 The script discovers mosquitto_sub from an explicit path, PATH, and common install locations, optionally installs Mosquitto with winget, refreshes the current PATH, and persists the user PATH when needed.
- [ ] #3 The script creates a timestamped run folder with telnet.log, mqtt.log, and summary.txt, enables MQTT telnet debug only when the banner indicates it is off, and records errors/status details.
- [ ] #4 scripts/README.md documents the diagnostic script briefly.
- [ ] #5 PowerShell parse validation passes for the new script.
<!-- AC:END -->

## Definition of Done
<!-- DOD:BEGIN -->
- [ ] #1 Backlog task includes branch metadata, coding-agent metadata, implementation notes, validation evidence, and final summary.
<!-- DOD:END -->
