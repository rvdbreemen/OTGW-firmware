---
id: TASK-1941
title: 'SAT flow offset: REST/UI range says 0-10 but the setter clamps to 0.5-10'
status: To Do
assignee: []
created_date: '2026-11-14T16:37:00Z'
labels:
  - bug
  - settings
  - rest
  - sat
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 993000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found 2026-11-14 by the coordinator during the Q-346a audit (Sergeant D, #alpha-testing 1571200241556914279). restAPI.ino:4407 publishes satflowoffset with range 0..10 (addNum(..., "f", 0, 10)), so the settings UI offers 0. The setter settingStuff.ino:1163 runs clampSettingFloat(newValue, current, 0.5f, 10.0f); clampSettingFloat (settingStuff.ino:788-796) uses constrain(), so a 0 (or 0.1-0.4) is silently stored as 0.5 instead of being refused or shown. Fix: make the REST/UI range match the clamp (min 0.5). No SAT behaviour change: the stored value and the clamp stay as they are.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 GET /api/v2/settings reports satflowoffset with min 0.5 and max 10 (curl output)
- [ ] #2 A host or web UI test fails on the old range (min 0) and passes on the fix
- [ ] #3 Setting 0.5 and 10 via the settings POST still works; the stored value matches (curl)
<!-- AC:END -->
