---
id: TASK-1389
title: >-
  MQTT MaxRelModLevelSetting/override always 0.00 and declared as temperature
  (degrees C) in HA discovery
status: To Do
assignee: []
created_date: '2026-10-25T12:53:00Z'
updated_date: '2026-10-27T23:46:00Z'
labels:
  - needs-maintainer-ok
milestone: 2.0.0
dependencies:
  - TASK-1412.3
references:
  - TASK-1412
ordinal: 528000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Seen in gruijter's alpha.504 capture 2026-10-25 (verifier of wf_4831cf35): the topic publishes 0.00 every minute, also after MM=100 and while the PIC rewrote MsgID 14 to 100 %; its HA discovery config declares device_class temperature and unit degrees C for a percentage. Source not traced yet.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Root cause traced with file:line
- [ ] #2 Topic reflects the active MM override (or is documented otherwise) and discovery declares a percentage (host test or capture, red on HEAD)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-25 tester feedback: needs Robert's OK before implementation (testers have no admin control).

2026-10-28 execution sequencing: reused by TASK-1412. Required prerequisites are encoded as dependencies; existing status, queue ordinal, decisions and approval requirements are preserved. Do not resume affected control changes before the scoped approved contract and red tests. Later integration, simulation, bench acceptance and field evidence are tracked by the epic; this metadata update is not implementation or validation.
<!-- SECTION:NOTES:END -->
