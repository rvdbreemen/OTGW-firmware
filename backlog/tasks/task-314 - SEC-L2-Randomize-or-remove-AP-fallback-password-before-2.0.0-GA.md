---
id: TASK-314
title: '[SEC-L2] Randomize or remove AP fallback password before 2.0.0 GA'
status: To Do
assignee: []
created_date: '2026-04-18 19:23'
labels:
  - security
  - review-2026-04-18
dependencies: []
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
networkStuff.ino:232 hard-codes AP password otgw123 inside _VERSION_PRERELEASE gate. WiFiManager already handles first-boot provisioning. Anyone in RF range with prerelease build can associate.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 AP fallback either removed entirely or password randomized per device
- [ ] #2 Randomization derived from chipId with a build-time salt and surfaced on OLED/serial only
- [ ] #3 evaluate.py refuses to build a non-prerelease tag if _VERSION_PRERELEASE paths remain reachable
<!-- AC:END -->
