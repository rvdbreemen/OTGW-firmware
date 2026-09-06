---
id: TASK-1132
title: The water-meter docs state the MsgID 19 precondition but never the remedy
status: In Progress
assignee:
  - '@claude'
created_date: '2026-09-06 16:42'
updated_date: '2026-09-06 16:45'
labels:
  - documentation
dependencies: []
priority: medium
ordinal: 216000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
The cumulative DHW water total (TASK-1091) is announced to Home Assistant only after a MsgID 19 frame has actually decoded. That gate is deliberate (TASK-1093, ADR-093).

Premise correction, recorded because the task was opened on the wrong one: RELEASE_NOTES_1.7.5.md:69, RELEASE_GITHUB_1.7.5.md:26 and README.md:17 all DO state that precondition. Verified by reading them. The omission was in the Discord release announcement, which condensed the item to 'Nieuw: een cumulatieve warmwatermeter voor het Energy-dashboard van Home Assistant. Geen helper en geen YAML nodig.' That message is sent and is not being retracted.

What survives is the half none of the three documents carries: what the reader should DO about it. They state the fact and stop. A thermostat that never requests MsgID 19 will never produce the entity, and the gateway does not poll that id on its own, so the remedy is AA=19 (addalternative), which makes the gateway substitute the request itself. That command appears in the repository exactly once, as a row in the command table at docs/api/MQTT.md:474, with no connection to the water meter.

Observed cost of the gap: stefan_24213 in Discord #nederlandse-ondersteuning on 2026-09-06 updated to 1.7.5, saw no sensor, and asked. The maintainer answered by hand with MsgID 19 and the AA=19 hint. Every later user on a bus without DHW flow data needs the same answer.

Documentation only: no firmware change, no new build, no reflash.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 The reader is told what to do about it: AA=19 makes the gateway request MsgID 19 itself when the thermostat never asks
- [ ] #2 The published GitHub release body for v1.7.5 carries the same correction
- [ ] #3 README.md, RELEASE_NOTES_1.7.5.md and RELEASE_GITHUB_1.7.5.md each name the remedy alongside the precondition they already state
- [ ] #4 The remedy is stated accurately: the gateway does not poll MsgID 19, AA=19 adds it to the alternative-message table, and that table is finite (NS - No Space)
<!-- AC:END -->
