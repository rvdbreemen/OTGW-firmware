---
id: TASK-616
title: Add openHAB and Domoticz integration guides (HA discovery consumers)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-05-17 07:22'
updated_date: '2026-05-17 07:22'
labels:
  - documentation
  - mqtt
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
OTGW already emits Home Assistant MQTT Discovery under a configurable prefix (settings.mqtt.sHaprefix). openHAB (MQTT binding + HomeAssistant MQTT Components binding) and Domoticz (MQTT Auto Discovery Client hardware) both consume that same HA discovery format. No firmware change is needed to support them - what is missing is user-facing documentation. Add two docs/guides pages mirroring EMS-ESP's openHAB/Domoticz docs, and cross-link them from the MQTT API reference.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 docs/guides/OPENHAB.md exists and documents: required openHAB bindings (MQTT + HomeAssistant MQTT Components), how the discovery prefix maps to settings.mqtt.sHaprefix (default homeassistant), Thing grouping via device identifiers, and the trusted-LAN/no-auth security note
- [ ] #2 docs/guides/DOMOTICZ.md exists and documents: the MQTT Auto Discovery Client hardware setup, discovery-prefix mapping to settings.mqtt.sHaprefix, and an explicit do-not-use warning for the legacy domoticz/in idx protocol (YAGNI)
- [ ] #3 Both guides accurately reflect the discovery topic pattern {haprefix}/{component}/{node_id}/{object_id}/config and cross-link to docs/api/MQTT.md Home Assistant Auto-Discovery section
- [ ] #4 docs/api/MQTT.md links out to the two new guides
- [ ] #5 Change is docs-only (no src/ firmware files touched) so it is version-bump and ADR exempt
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Write docs/guides/OPENHAB.md: bindings to install, prefix=settings.mqtt.sHaprefix, device-identifier Thing grouping, ADR-071 topic shape note, trusted-LAN security caveat, troubleshooting
2. Write docs/guides/DOMOTICZ.md: MQTT Auto Discovery Client hardware setup, prefix mapping, explicit do-not-use legacy domoticz/in idx warning, troubleshooting
3. Cross-link both to docs/api/MQTT.md HA Auto-Discovery section; add reciprocal links from MQTT.md
4. Commit (docs-only, bump/ADR exempt), push, open draft PR
<!-- SECTION:PLAN:END -->
