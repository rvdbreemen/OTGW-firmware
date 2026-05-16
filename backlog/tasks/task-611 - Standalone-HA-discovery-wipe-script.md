---
id: TASK-611
title: Standalone HA discovery wipe script
status: Done
assignee:
  - '@claude'
created_date: '2026-05-16 08:59'
updated_date: '2026-05-16 09:10'
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
- [x] #1 Script is standalone (stdlib-only, no pip install required) and runnable directly with python3
- [x] #2 Device unique_id and ha_prefix are discovered via the OTGW REST API; hostname defaults to OTGW.local and can be overridden
- [x] #3 Only retained discovery '/config' topics scoped to the resolved device unique_id are targeted; other devices on a shared broker are never touched
- [x] #4 User must explicitly authorize the wipe via an interactive confirmation dialog before any topic is cleared; a dry-run mode lists topics without wiping
- [x] #5 Retained discovery topics are cleared by publishing zero-length retained payloads; script reports the count wiped
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

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Implemented scripts/wipe_ha_discovery.py (stdlib-only MQTT 3.1.1 subset). REST discovery tries /api/v2/debug then /api/v2/settings (HTTP basic auth optional) then /api/v2/device/info (unauth, derives default otgw-<MAC> unique_id). Topic scoping: last segment == config AND ha_prefix head AND unique_id is a path segment. Confirmation requires typing WIPE; --dry-run and --yes supported. Added auto-installing wrappers wipe_ha_discovery.sh / .bat per user request. Verified end-to-end with an in-process MQTT broker stub: only the target device config topics cleared (other device excluded), clears use retain=1 + empty payload; dry-run and confirm-decline publish nothing.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Add a standalone, dependency-free utility to wipe an OTGW's retained Home Assistant MQTT discovery topics.

What & why: After removing/renaming an OTGW, stale retained <ha_prefix>/<component>/<unique_id>/.../config topics keep ghost entities alive in Home Assistant. This script clears exactly those, scoped to one device.

Changes:
- scripts/wipe_ha_discovery.py: stdlib-only (no pip). Discovers unique_id/ha_prefix/broker/port/mqtt_user via OTGW REST (/api/v2/debug -> /api/v2/settings -> /api/v2/device/info; host defaults to OTGW.local, all values overridable by CLI). Minimal MQTT 3.1.1 client (CONNECT/SUBSCRIBE/PUBLISH/DISCONNECT). Collects retained topics, matches only last-segment "config" topics that contain the resolved unique_id, requires an interactive "WIPE" confirmation (or --yes), supports --dry-run, then clears each topic with a zero-length retained publish and reports the count.
- scripts/wipe_ha_discovery.sh / .bat: launchers that locate Python 3 and auto-install it (apt/dnf/yum/pacman/zypper/apk/brew; winget/choco on Windows) when absent, with WIPE_HA_NO_AUTOINSTALL=1 opt-out and manual-install fallback messaging.

User impact: other devices on a shared broker are never touched; nothing is changed without explicit confirmation.

Tests: py_compile + sh -n syntax; argparse --help; end-to-end against an in-process MQTT broker stub proving (a) only the target device's config topics are cleared, (b) clears use retain flag + empty payload, (c) --dry-run and a declined confirmation publish nothing.

Risks/follow-ups: .local resolution depends on host mDNS (script prints guidance and accepts --host <ip>); broker host/MQTT user are only auto-discovered when the OTGW debug/settings endpoint is reachable (HTTP password supplied if set) - otherwise pass --broker. Tooling-only change (scripts/**): exempt from version bump and firmware build/evaluator gates per project policy.
<!-- SECTION:FINAL_SUMMARY:END -->
