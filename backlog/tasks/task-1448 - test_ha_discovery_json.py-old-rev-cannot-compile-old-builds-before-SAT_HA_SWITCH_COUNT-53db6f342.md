---
id: TASK-1448
title: >-
  test_ha_discovery_json.py --old-rev cannot compile old builds before
  SAT_HA_SWITCH_COUNT (53db6f342)
status: To Do
assignee: []
created_date: '2026-10-29T06:18:00Z'
labels:
  - test
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 608000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found while fixing TASK-1443 (2026-10-29): test/host/test_ha_discovery_json.py --old-rev 8795bacc0 fails to compile its OLD build, because test_ha_discovery_json.cpp:209-211 uses SAT_HA_SWITCH_COUNT, introduced in 53db6f342. The old-vs-fix mode only works for revisions after that commit. Make the harness tolerate older revisions (guard/fallback) or document the lower bound of --old-rev.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 --old-rev against a revision before 53db6f342 either compiles or stops with a clear message naming the lower bound
<!-- AC:END -->
