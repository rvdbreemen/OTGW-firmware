---
id: TASK-535
title: >-
  Docs/fix: duplicate HA entities after firmware upgrade — stale retained MQTT
  discovery topics
status: To Do
assignee: []
created_date: '2026-05-04 06:11'
updated_date: '2026-05-04 06:12'
labels:
  - bug
  - needs-info
dependencies: []
references:
  - 'Discord #beta-testing'
  - user _reuzenpanda_
  - '2026-04-30'
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Users (stefan, _reuzenpanda_) report duplicate HA entities (e.g. two 'OTGW_Room_Temperature' sensors) after upgrading firmware. Root cause: retained MQTT discovery topics from the old firmware version remain on the broker. HA picks up both old and new discovery configs and appends _2 to the duplicate. The automated wipe-on-OTA feature was designed to solve this but was withdrawn (ADR-067, too complex for ESP8266 constraints). Users must manually clean up stale retained topics via MQTT Explorer or similar. This task tracks: (1) clear user documentation on how to do this cleanup, and (2) evaluating whether a simpler targeted cleanup (e.g. just the device-level discovery prefix, not 1200 individual topics) is feasible.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Release notes / wiki page documents the manual cleanup steps for stale HA discovery topics after firmware upgrade
- [ ] #2 No duplicate HA entities after a clean flash + MQTT broker cleanup
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-05-03: stefan screenshot shows two identical 'OTGW_Room_Temperature 21.9°C' sensors in HA MQTT integration. He confirmed bSeparateSources = OFF. Maintainer confirmed this is HA renaming due to naming conflict from stale retained discovery topics. The wipe-on-OTA feature (ADR-067) was designed to solve this but removed after testing (too fragile on ESP8266). Waiting for: nothing blocking for docs task; for code fix track complexity vs. benefit.
<!-- SECTION:NOTES:END -->
