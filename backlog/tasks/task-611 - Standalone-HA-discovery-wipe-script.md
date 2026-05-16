---
id: TASK-611
title: Standalone HA discovery wipe script
status: In Progress
assignee:
  - '@claude'
created_date: '2026-05-16 08:59'
updated_date: '2026-05-16 08:59'
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

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Probe OTGW REST API: GET /api/v2/device/info (no auth) for hostname+macaddress; try /api/v2/settings & /api/v2/debug (HTTP basic auth if creds given) for exact mqttuniqueid/mqtthaprefix/broker/port/user.
2. Resolve device id: use settings.mqtt.unique_id when available, else derive default otgw-<MAC w/o colons, upper>; ha_prefix default homeassistant.
3. Zero-dependency MQTT 3.1.1 client (stdlib socket): CONNECT (opt user/pass), SUBSCRIBE <prefix>/#, collect retained PUBLISHes until idle timeout.
4. Filter topics: last segment == config AND resolved unique_id is a path segment (scoped to this device only).
5. Confirmation dialog: list matched topics, require explicit typed yes; --dry-run lists only; --yes bypass for automation.
6. Wipe: publish zero-length retained payload per matched topic; report count.
7. Place at scripts/wipe_ha_discovery.py; manual smoke (argparse --help, dry-run path).
<!-- SECTION:PLAN:END -->
