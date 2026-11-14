---
id: TASK-1941
title: 'SAT flow offset: REST/UI range says 0-10 but the setter clamps to 0.5-10'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-14T16:37:00Z'
updated_date: '2026-11-14T22:09:00Z'
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
- [x] #2 A host or web UI test fails on the old range (min 0) and passes on the fix
- [ ] #3 Setting 0.5 and 10 via the settings POST still works; the stored value matches (curl)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Fix (ba 2026-11-14, on the 1937 stack): a scan of every clamped setter (settingStuff.ino clampSetting*(newValue, cur, min, max)) against its addNum() range in restAPI.ino found 10 settings whose advertised range was wider than the clamp, all SAT: satflowoffset 0-10 vs 0.5-10, satovershootmargin 0-5 vs 0.5-5, satsimheatrate/satsimcoolrate 0-5 vs 0.01-5, satthermalcoeff 0-1 vs 0.005-0.3, satsolarminrise 0-5 vs 0.1-5, satsolaroffset 0-10 vs 0.5-10, satpvboostdeltac 0-5 vs 0.5-5, satmaxpressdrop 0-2 vs 0.05-2, satboilerefficiency 0-1 vs 0.5-1. All ten advertised ranges now equal the clamp; the clamps and stored values are unchanged (no SAT behaviour change). AC2: tests/test_settings_rest_range_matches_clamp.py pairs every clamp with its addNum entry by name: old (OTGW_RANGE_REV=HEAD) FAIL, fix 3/3 OK; 11 other settings/SAT range tests OK; evaluate --quick 0 FAIL. AC1/AC3 (curl GET ranges, POST 0.5 and 10) on the stack bench, brief item 9.
<!-- SECTION:NOTES:END -->
