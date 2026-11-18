---
id: TASK-2022
title: 'docs: ch04/h04 describe the gateway override sensors (ADR-118)'
status: To Do
assignee: []
created_date: '2026-11-18T10:34:00Z'
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
- [ ] #1 ch04 and h04 have a short section on the gateway override sensors: which IDs, the entity per ID, what the value means, when it appears and disappears; each claim checked against the code
- [ ] #2 No em dashes, no OTGW32; NL in idiomatic Dutch
<!-- AC:END -->
