---
id: TASK-1448
title: >-
  test_ha_discovery_json.py --old-rev cannot compile old builds before
  SAT_HA_SWITCH_COUNT (53db6f342)
status: Done
assignee:
  - '@claude'
created_date: '2026-10-29T06:18:00Z'
updated_date: '2026-11-04T05:46:00Z'
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
- [x] #1 --old-rev against a revision before 53db6f342 either compiles or stops with a clear message naming the lower bound
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Triage 2026-11-02 (beta.5, origin/dev 6c41cc696) in code: no guard for --old-rev in test/host/test_ha_discovery_json.py:559/622. Agent-doable.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
test_ha_discovery_json.py --old-rev now stops with a clear lower-bound message for revisions older than c39b8e9f7 (the task named 53db6f342, but streamSatNumberDiscovery came later in c39b8e9f7). Evidence: --old-rev 8795bacc0 / a632e8e68 / 53db6f342 exit 1 with the message; --old-rev c39b8e9f7 and HEAD pass the range check. Pre-existing, not part of this task: an in-range --old-rev later crashes at :666 (IndexError) because the old-vs-fix logic assumes the OLD side has the defect.
<!-- SECTION:FINAL_SUMMARY:END -->
