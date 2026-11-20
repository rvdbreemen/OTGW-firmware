---
id: TASK-2022
title: 'docs: ch04/h04 describe the gateway override sensors (ADR-118)'
status: Done
assignee:
  - '@claude'
created_date: '2026-11-18T10:34:00Z'
updated_date: '2026-11-18T11:21:00Z'
labels:
  - docs
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 1037000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found in TASK-2021: the Home Assistant chapters ch04 (EN) and h04 (NL) do not mention the gateway override sensors at all. The firmware announces one sensor per override-capable MsgID (IDs 1, 8, 9, 14, 16, 39, 56, 57) on the Gateway device, state topic {TopTopic}/{label}/override, retained, refreshed every 60 s while active, stale after 10 min, cleared after a reconnect when no longer active (TASK-1991). Sources: docs/api/MQTT.md 'Gateway override topics' (ADR-118, TASK-1389), MQTTHaDiscovery.cpp streamOverrideSensorDiscovery, OTGW-Core.ino publishActiveOverrides / staleOverrideClearStep. Docs only.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 ch04 and h04 have a short section on the gateway override sensors: which IDs, the entity per ID, what the value means, when it appears and disappears; each claim checked against the code
- [x] #2 No em dashes, no OTGW32; NL in idiomatic Dutch
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-18 (otgw-docs): new section 'Gateway Override Sensors' in ch04 (after Number Entity) and 'Override-sensoren van de gateway' in h04. Every claim against the code: IDs and Gateway device MQTTstuff.ino:3427-3436 (case 1, 8, 9, 14, 16, 39, 56, 57; 27 excluded for the number entity :3421-3425 and MQTTHaDiscovery.cpp streamOverrideSensorDiscovery 'if (otid == 27) return false'); labels from messageIDToString() = OTmap labels (OTGW-Core.ino:1860, OTGW-Core.h OTmap rows 1/8/9/14/16/39/56/57: TSet, TsetCH2, TrOverride, MaxRelModLevelSetting, TrSet, TrOverride2, TdhwSet, MaxTSet); name '<label> Override', stat_t <pub>/<label>/override, temperature class + degC except 14 = % and mdi:percent-outline (streamOverrideSensorDiscovery compose); value = gateway answer override or substituted write (OTGW-Core.ino recordOTOverride calls ~:2977-2982); refresh 60 s (MQTTstuff.ino:1510 timerMQTToverridepublish, :1660); stale 10 min -> empty retained (OTGW-Core.h:877 OVERRIDE_ACTIVE_TIMEOUT 600000, publishActiveOverrides, TASK-1985); reboot sweep (TASK-1991, OTGW-Core.ino staleOverrideClearStep). Left out as unverified: how HA renders an entity after an empty payload. No em dashes, no OTGW32.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
ch04/h04 got a 'Gateway Override Sensors' section (EN) and 'Override-sensoren van de gateway' (NL): IDs, entity, unit, value meaning, announce, 60 s refresh, 10 min stale clear, reboot sweep; each claim with file:line in the notes. Not verified: how HA displays the sensor after the empty retained message (left out of the text).
<!-- SECTION:FINAL_SUMMARY:END -->
