---
id: TASK-2024
title: >-
  HTTPS self-update: the gateway downloads a new release from GitHub itself
  (option C of TASK-2023, 2.1.0)
status: To Do
assignee: []
created_date: '2026-11-18T12:19:00Z'
updated_date: '2026-11-18T12:25:00Z'
labels:
  - feature
milestone: 2.1.0
dependencies: []
priority: low
ordinal: 1039000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert 2026-11-18 (DM 1572585865589428318): "B en neem taak C op voor een latere release 2.1.0". Option C of Sergeant D's request (#dev-sat-mqtt 1572576499674914927): the gateway downloads and installs a new release straight from GitHub. GitHub serves release assets over HTTPS only, so this needs an outbound TLS client: ADR-218 (outbound TLS client requests on boards with PSRAM, inbound stays plain HTTP, amends ADR-003) was decided on 2026-11-07 and parked on 2026-11-08; unpark it or supersede it first. Constraints: app slot flash budget (TASK-1955), PSRAM boards only per ADR-218, certificate trust and upkeep, safeboot recovery path. Option B (version check over plain HTTP) is TASK-2023 for now.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 ADR-218 unparked or superseded with the design (trust store, PSRAM-only, flash cost measured)
- [ ] #2 A self-update from a GitHub release works on a PSRAM board, with safeboot recovery verified on the bench
- [ ] #3 Boards without PSRAM keep the manual /update path, documented
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Robert in #dev-sat-mqtt 2026-11-18 12:06 UTC (https://discord.com/channels/812969634638725140/1105556725714649128/1572583679895797861), verbatim: "For 2.0.0 we do B and for 2.1.0 a POC for updating from GitHub directly." So 2.1.0 starts with a proof of concept (POC) of the direct GitHub update.
<!-- SECTION:NOTES:END -->
