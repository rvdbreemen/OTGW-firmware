---
id: TASK-1085
title: Implement the cumulative DHW water total (blocked on ADR-090 acceptance)
status: To Do
assignee: []
created_date: '2026-08-24 20:22'
labels:
  - enhancement
dependencies: []
priority: low
ordinal: 186000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Follow-on from TASK-1081 and GitHub #675. The rate sensor is now correctly typed as volume_flow_rate, but the Energy dashboard needs a cumulative volume entity that does not exist yet. ADR-090 proposes it and answers the design questions (time x flow zero-hold with a capped interval, resume-and-undercount on unclean reboot, own file not settings.ini, write on delta >= 10 L or a 15-minute floor, paired REST+MQTT reset). BLOCKED: ADR-090 is still Proposed and needs maintainer acceptance before implementation.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 ADR-090 is Accepted before any code is written
- [ ] #2 Cumulative total published as device_class water, unit L, state_class total_increasing, and selectable as an Energy dashboard water source on a live HA instance
- [ ] #3 Accumulation is elapsed-time x flow, hooked at both state write sites, and one real litre of flow is counted exactly once (bench-verified, including OTDirect master mode with a thermostat on the 2.0.0 peer)
- [ ] #4 Counter persists across reboot, never decreases, and is resettable through the paired REST and MQTT surface
<!-- AC:END -->
