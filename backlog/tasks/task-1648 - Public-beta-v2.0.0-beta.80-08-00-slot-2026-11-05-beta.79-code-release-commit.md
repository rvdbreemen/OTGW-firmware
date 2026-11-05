---
id: TASK-1648
title: >-
  Public beta v2.0.0-beta.80 (08:00 slot 2026-11-05): beta.79 code + release
  commit
status: Done
assignee:
  - '@claude'
created_date: '2026-11-05T05:26:00Z'
updated_date: '2026-11-05T12:56:00Z'
labels:
  - release
  - beta
dependencies: []
priority: high
ordinal: 772000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Public beta from the 12-hour slot (Robert 2026-11-02/03 rules). Candidate beta.79 (TASK-1608 HA names, TASK-1609 flow dots) + docs-only release commit bumped to beta.80. 1 h MQTT soak on .63.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Release commit: notes, CHANGELOG rollover, bump; evaluate --quick 0 failed
- [ ] #2 esp32-combo build SUCCESS, OTA on .63, settings unchanged except keys unknown to beta.80
- [ ] #3 1 h soak with MQTT: bootcount = flash + 2, no crashlog, heap flat
- [ ] #4 GitHub prerelease --latest=false with matching SHA256SUMS; latest stays 1.x
- [ ] #5 #alpha-testing zip post and #beta-testing announcement
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-05: release commit 6c645648e built (esp32-combo SUCCESS), OTA on .63 06:38, 1 h MQTT soak 06:39-07:44: bootcount 143 throughout (= flash + 2), no crashlog, MQTT up, min free 24288 flat, freeheap 56620 -> 55848 (-772 B, same shape as beta.76 baseline -452 B/45 min per the lead). Slot missed: this session waited on a maintainer question about the heap drift; Robert later cancelled the 08:00 beta (~09:35, via the lead). Nothing tagged or published. The commit is in origin/dev (CHANGELOG section 2.0.0-beta.80 and RELEASE_NOTES_2.0.0-beta.80.md exist for an unpublished version): fold into the 20:00 release.

2026-11-05: superseded by TASK-1657 (v2.0.0-beta.95 published ~13:55; the cancelled beta.80 section and notes folded into beta.95).
<!-- SECTION:NOTES:END -->
