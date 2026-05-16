---
id: TASK-611
title: Standalone HA discovery wipe script
status: To Do
assignee: []
created_date: '2026-05-16 08:59'
labels:
  - tooling
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Provide a standalone Python utility that clears retained Home Assistant MQTT discovery (config) topics for a specific OTGW device. The script discovers the device unique_id / ha_prefix via the OTGW REST API (defaulting to OTGW.local), connects to the MQTT broker, and wipes only this device's retained discovery config topics after explicit user confirmation.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Script is standalone (stdlib-only, no pip install required) and runnable directly with python3
- [ ] #2 Device unique_id and ha_prefix are discovered via the OTGW REST API; hostname defaults to OTGW.local and can be overridden
- [ ] #3 Only retained discovery '/config' topics scoped to the resolved device unique_id are targeted; other devices on a shared broker are never touched
- [ ] #4 User must explicitly authorize the wipe via an interactive confirmation dialog before any topic is cleared; a dry-run mode lists topics without wiping
- [ ] #5 Retained discovery topics are cleared by publishing zero-length retained payloads; script reports the count wiped
<!-- AC:END -->
