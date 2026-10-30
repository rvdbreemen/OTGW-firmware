---
id: TASK-1501
title: >-
  SAT: external outside temperature first for its whole valid hour (entry 139,
  Q66 A)
status: To Do
assignee: []
created_date: '2026-10-30T06:11:00Z'
updated_date: '2026-10-30T06:59:00Z'
labels:
  - sat sergeant-d
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 657000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeant D 2026-10-30 Q66 A (entry 139): order external sensor (MQTT/REST, valid < 1 h), MsgID 27, weather, MsgID 27 reading 0.0, restored value. The firmware still prefers the external value only for 10 minutes (SAT_STALE_OUTDOOR_MS, SATcontrol.ino:142, :1878, :1900 at 8ded885eb). Found by the coordinator 2026-10-30 after TASK-1482 was closed with a wrong 'matches the implementation' note.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Host test on the real source selection: an external value 30 min old wins over MsgID 27 and weather (red on 8ded885eb), a 61 min old one does not
- [ ] #2 The restored-value and MsgID 27 = 0.0 fallbacks keep their order; docs/sat/firmware-sat-spec.md updated
- [ ] #3 build + evaluate green
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-30: Sergeant D entry 146: must be in beta 1 (TASK-1504, target Sun 2026-11-01). Priority raised to high.
<!-- SECTION:NOTES:END -->
