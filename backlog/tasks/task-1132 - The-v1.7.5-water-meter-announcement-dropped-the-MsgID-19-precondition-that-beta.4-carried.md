---
id: TASK-1132
title: >-
  The v1.7.5 water-meter announcement dropped the MsgID 19 precondition that
  beta.4 carried
status: In Progress
assignee:
  - '@claude'
created_date: '2026-09-06 16:42'
updated_date: '2026-09-06 16:43'
labels:
  - documentation
dependencies: []
priority: medium
ordinal: 216000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
The cumulative DHW water total (TASK-1091) is announced to Home Assistant only after a MsgID 19 frame has actually decoded. That gate is deliberate (TASK-1093, ADR-093): without it every gateway whose thermostat never requests MsgID 19 gains a retained config for an entity that sits at 'unknown' forever.

The beta.4 announcement told users this in so many words: 'It appears only once your thermostat has actually requested MsgID 19, so no entity means your bus carries no DHW flow data, not a broken build.' When the release text was condensed for stable v1.7.5 that sentence was dropped, leaving 'New: a cumulative hot water total for the Home Assistant Energy dashboard. No helper and no YAML needed.'

Result, observed once already: stefan_24213 in Discord #nederlandse-ondersteuning on 2026-09-06 updated to 1.7.5, saw no sensor, and asked whether he had to do something himself. The answer is that his boiler and thermostat have to exchange MsgID 19, and that AA=19 makes the gateway ask for it when the thermostat does not. Every subsequent user on a bus without DHW flow data will hit the same wall.

CHANGELOG.md already states the precondition correctly under [1.7.5]; the omission is in the reader-facing release notes and the published GitHub release body. This is documentation only: no firmware change, no new build, no reflash.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 RELEASE_NOTES_1.7.5.md states that the entity appears only after a MsgID 19 frame decodes, and that no entity means the bus carries no DHW flow data rather than a broken build
- [ ] #2 The reader is told what to do about it: AA=19 makes the gateway request MsgID 19 itself when the thermostat never asks
- [ ] #3 The published GitHub release body for v1.7.5 carries the same correction
- [ ] #4 README wording for the feature does not promise an entity without naming the precondition
<!-- AC:END -->
