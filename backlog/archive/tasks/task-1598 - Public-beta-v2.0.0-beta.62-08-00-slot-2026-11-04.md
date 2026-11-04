---
id: TASK-1598
title: Public beta v2.0.0-beta.62 (08:00 slot 2026-11-04)
status: Done
assignee:
  - '@claude'
created_date: '2026-11-04T05:12:00Z'
updated_date: '2026-11-04T06:30:00Z'
labels:
  - release
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 741000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Morning public beta: Robert 2026-11-04 'next public beta around 8am', soak starts ~06:30 with the newest beta then available, 60 min MQTT soak, release at 08:00. Candidate beta.62 (TASK-1447, TASK-1462).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Release notes + CHANGELOG rollover committed (delta since beta.60, TASK-1462 HA entity change flagged)
- [x] #2 Release-commit build OTA on the Classic+S3 unit, 60 min MQTT soak: boot = flash+2, no crashlog, flat heap
- [x] #3 Tag, prerelease --latest=false, assets + SHA verified, latest stays 1.x
- [x] #4 #beta-testing announcement sent
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
v2.0.0-beta.62 published 2026-11-04 ~07:30 local (prerelease, latest stays v1.7.8): tag on release commit c6716bf44; 7 assets equal build/, downloaded flash zip sha256 = SHA256SUMS (41edb060...). Soak: release image on the Classic+S3 unit 06:25-07:26 local with MQTT, 13/13 samples boot 100 (= flash+2), heap 56336-56820 flat, no crashlog, dropped 0. #alpha-testing zip of beta.62 posted earlier (msg 1567405917907984414); #beta-testing announcement (text approved by Robert) sent and read back (msg 1567425776708751422).
<!-- SECTION:FINAL_SUMMARY:END -->
