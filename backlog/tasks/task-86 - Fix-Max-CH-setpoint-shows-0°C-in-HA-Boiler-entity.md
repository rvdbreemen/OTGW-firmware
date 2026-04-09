---
id: TASK-86
title: 'Fix: Max CH setpoint shows 0°C in HA Boiler entity'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-08 20:51'
updated_date: '2026-04-09 20:08'
labels:
  - bug
  - mqtt
  - home-assistant
dependencies: []
references:
  - 'https://github.com/rvdbreemen/OTGW-firmware/issues/543'
  - 'Discord #beta-testing, user andrebrait, 2026-04-07/08'
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
The maximum CH water setpoint is correctly visible in the web UI and the HA Thermostat entity, but the HA Boiler entity always shows 0°C. Reported by andrebrait on Discord #beta-testing. Present since at least v1.0.0.

**Status: waiting for information — do not pick up yet.**

Needed before work can start:
- Telnet debug logs from andrebrait while issue is present (requested 2026-04-08)
- Confirmation of exact MQTT topics the Boiler entity subscribes to vs. what is published

Suspected cause: OT message ID mapping issue — value reaches firmware correctly but may be published to wrong MQTT source topic or Boiler entity HA auto-discovery references wrong topic.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 HA Boiler entity shows correct max CH setpoint (matching web UI value)
- [x] #2 HA Thermostat entity still shows correct value after fix
- [x] #3 Telnet logs confirm correct MQTT publish path
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Checkout fix branch from dev
2. Bump patch version
3. Change resolveSourceIndex() in MQTTstuff.ino: split OTGW_ANSWER_THERMOSTAT from OTGW_REQUEST_BOILER so it maps to sourceIndex 1 (boiler) instead of 2 (gateway)
4. Build and evaluate
5. Commit and report
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-04-08: andrebrait shared HA integration debug logs (via attachment in Discord #beta-testing). However, maintainer clarified that firmware-side telnet logs are needed (not HA logs) — see wiki link sent by number3nl. Task remains needs-info until telnet logs arrive.

2026-04-09: andrebrait reloaded HA integration and shared new logs (attachment in #beta-testing). Still HA-side logs, not firmware telnet logs. Maintainer explicitly requested telnet logs via wiki link. Task remains needs-info.

2026-04-09: Root cause confirmed from telnet logs provided by andrebrait. ID 57 (MaxTSet) arrives only as OTGW_ANSWER_THERMOSTAT (A-prefix) — no direct boiler B-prefix. resolveSourceIndex() maps A-prefix to sourceIndex 2 (gateway), so boiler source topic never receives the value. Fix: separate OTGW_ANSWER_THERMOSTAT to sourceIndex 1 (boiler).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Fixed OTGW_ANSWER_THERMOSTAT source mapping in MQTTstuff.ino resolveSourceIndex(). A-prefix messages were routed to 'gateway' source (index 2) instead of 'boiler' source (index 1). For ID 57 (MaxTSet), the OTGW always answers the thermostat directly without a boiler B-prefix, so the boiler source topic never got the value. HA Boiler entity showed 0°C while web UI and Thermostat entity showed 90°C. One-line fix: separate OTGW_ANSWER_THERMOSTAT from OTGW_REQUEST_BOILER case. Build and eval pass. Branch: fix-issue-maxtset-boiler-source.
<!-- SECTION:FINAL_SUMMARY:END -->
